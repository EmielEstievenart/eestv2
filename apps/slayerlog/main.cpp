#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#include "command_line_parser.hpp"
#include "file_watcher.hpp"

int main(int argc, char** argv)
{
    const auto config = slayerlog::parse_command_line(argc, argv);
    std::vector<std::string> lines;
    std::vector<std::string> paused_pending_lines;

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    screen.TrackMouse();
    int scroll_offset      = 0;
    int visible_line_count = 1;
    bool follow_bottom     = true;
    bool updates_paused    = false;
    bool paused_has_snapshot = false;
    ftxui::Box log_viewport_box;
    std::mutex lines_mutex;

    struct TextPosition
    {
        int line   = 0;
        int column = 0;
    };

    std::optional<TextPosition> selection_anchor;
    std::optional<TextPosition> selection_focus;
    bool selection_in_progress = false;

    auto line_with_prefix = [&](int index)
    {
        std::ostringstream output;
        output << index + 1 << " " << lines[static_cast<std::size_t>(index)];
        return output.str();
    };

    auto clear_selection = [&]
    {
        selection_anchor.reset();
        selection_focus.reset();
        selection_in_progress = false;
    };

    auto selection_bounds = [&]() -> std::optional<std::pair<TextPosition, TextPosition>>
    {
        if (!selection_anchor.has_value() || !selection_focus.has_value())
        {
            return std::nullopt;
        }

        auto start = *selection_anchor;
        auto end   = *selection_focus;
        if (end.line < start.line || (end.line == start.line && end.column < start.column))
        {
            std::swap(start, end);
        }

        return std::pair(start, end);
    };

    auto selection_text = [&]() -> std::string
    {
        const auto bounds = selection_bounds();
        if (!bounds.has_value() || lines.empty())
        {
            return {};
        }

        const auto [start, end] = *bounds;
        std::ostringstream output;
        for (int line_index = start.line; line_index <= end.line; ++line_index)
        {
            const auto rendered_line = line_with_prefix(line_index);
            const int line_start      = (line_index == start.line) ? start.column : 0;
            const int line_end =
                (line_index == end.line) ? end.column : static_cast<int>(rendered_line.size());
            const int clamped_start = std::clamp(line_start, 0, static_cast<int>(rendered_line.size()));
            const int clamped_end   = std::clamp(line_end, clamped_start, static_cast<int>(rendered_line.size()));

            output << rendered_line.substr(
                static_cast<std::size_t>(clamped_start),
                static_cast<std::size_t>(clamped_end - clamped_start));
            if (line_index != end.line)
            {
                output << '\n';
            }
        }

        return output.str();
    };

    auto mouse_to_text_position = [&](const ftxui::Mouse& mouse) -> std::optional<TextPosition>
    {
        if (lines.empty())
        {
            return std::nullopt;
        }

        if (mouse.x < log_viewport_box.x_min || mouse.x > log_viewport_box.x_max || mouse.y < log_viewport_box.y_min ||
            mouse.y > log_viewport_box.y_max)
        {
            return std::nullopt;
        }

        const int line_index = scroll_offset + (mouse.y - log_viewport_box.y_min);
        if (line_index < 0 || line_index >= static_cast<int>(lines.size()))
        {
            return std::nullopt;
        }

        const auto rendered_line = line_with_prefix(line_index);
        const int column         = std::clamp(mouse.x - log_viewport_box.x_min, 0, static_cast<int>(rendered_line.size()));
        return TextPosition{line_index, column};
    };

    auto apply_update = [&](const slayerlog::FileWatcher::Update& update)
    {
        if (update.kind == slayerlog::FileWatcher::Update::Kind::Snapshot)
        {
            lines = update.lines;
            if (follow_bottom)
            {
                scroll_offset = std::max(0, static_cast<int>(lines.size()) - visible_line_count);
            }
        }

        if (update.kind == slayerlog::FileWatcher::Update::Kind::Append)
        {
            lines.insert(lines.end(), update.lines.begin(), update.lines.end());
            if (follow_bottom)
            {
                scroll_offset = std::max(0, static_cast<int>(lines.size()) - visible_line_count);
            }
        }
    };

    auto flush_paused_updates = [&]
    {
        if (paused_has_snapshot)
        {
            slayerlog::FileWatcher::Update update;
            update.kind  = slayerlog::FileWatcher::Update::Kind::Snapshot;
            update.lines = paused_pending_lines;
            apply_update(update);
        }
        else if (!paused_pending_lines.empty())
        {
            slayerlog::FileWatcher::Update update;
            update.kind  = slayerlog::FileWatcher::Update::Kind::Append;
            update.lines = paused_pending_lines;
            apply_update(update);
        }

        paused_pending_lines.clear();
        paused_has_snapshot = false;
    };

    auto copy_to_clipboard = [&](const std::string& text) -> bool
    {
        if (text.empty())
        {
            return false;
        }

#ifdef _WIN32
        const int wide_length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (wide_length <= 0)
        {
            return false;
        }

        auto* memory = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(wide_length) * sizeof(wchar_t));
        if (memory == nullptr)
        {
            return false;
        }

        auto* wide_text = static_cast<wchar_t*>(GlobalLock(memory));
        if (wide_text == nullptr)
        {
            GlobalFree(memory);
            return false;
        }

        const int converted = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide_text, wide_length);
        GlobalUnlock(memory);
        if (converted <= 0)
        {
            GlobalFree(memory);
            return false;
        }

        if (!OpenClipboard(nullptr))
        {
            GlobalFree(memory);
            return false;
        }

        EmptyClipboard();
        if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr)
        {
            CloseClipboard();
            GlobalFree(memory);
            return false;
        }

        CloseClipboard();
        return true;
#else
        (void)text;
        return false;
#endif
    };

    auto clamp_scroll_offset = [&]
    {
        const int max_scroll_offset = std::max(0, static_cast<int>(lines.size()) - visible_line_count);
        scroll_offset               = std::clamp(scroll_offset, 0, max_scroll_offset);
    };

    auto max_scroll_offset = [&]
    {
        return std::max(0, static_cast<int>(lines.size()) - visible_line_count);
    };

    auto update_follow_bottom = [&]
    {
        follow_bottom = scroll_offset >= max_scroll_offset();
    };

    std::optional<slayerlog::FileWatcher> watcher;
    try
    {
        watcher.emplace(
            config.file_path,
            [&](const slayerlog::FileWatcher::Update& update)
            {
                {
                    std::lock_guard lock(lines_mutex);
                    if (updates_paused)
                    {
                        if (update.kind == slayerlog::FileWatcher::Update::Kind::Snapshot)
                        {
                            paused_pending_lines = update.lines;
                            paused_has_snapshot  = true;
                        }
                        else
                        {
                            paused_pending_lines.insert(
                                paused_pending_lines.end(), update.lines.begin(), update.lines.end());
                        }
                    }
                    else
                    {
                        apply_update(update);
                    }

                    if (!lines.empty())
                    {
                        if (selection_anchor.has_value())
                        {
                            selection_anchor->line = std::clamp(selection_anchor->line, 0, static_cast<int>(lines.size()) - 1);
                            selection_anchor->column = std::clamp(
                                selection_anchor->column, 0, static_cast<int>(line_with_prefix(selection_anchor->line).size()));
                        }

                        if (selection_focus.has_value())
                        {
                            selection_focus->line = std::clamp(selection_focus->line, 0, static_cast<int>(lines.size()) - 1);
                            selection_focus->column = std::clamp(
                                selection_focus->column, 0, static_cast<int>(line_with_prefix(selection_focus->line).size()));
                        }
                    }
                    else
                    {
                        clear_selection();
                    }

                    clamp_scroll_offset();
                    update_follow_bottom();
                }

                screen.PostEvent(ftxui::Event::Custom);
            });
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n';
        return 1;
    }

    std::atomic<bool> keep_running = true;
    std::thread watcher_thread(
        [&]
        {
            while (keep_running)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(config.poll_interval_ms));
                if (!keep_running)
                {
                    break;
                }

                try
                {
                    watcher->process_once();
                }
                catch (...)
                {
                    // Ignore transient read errors while another process is writing.
                }
            }
        });

    auto viewer = ftxui::Renderer(
        [&]
        {
            std::lock_guard lock(lines_mutex);
            visible_line_count = std::max(1, log_viewport_box.y_max - log_viewport_box.y_min + 1);
            if (follow_bottom)
            {
                scroll_offset = max_scroll_offset();
            }
            else
            {
                clamp_scroll_offset();
                update_follow_bottom();
            }

            // Render only the visible window of lines so scrolling is controlled
            // entirely by scroll_offset instead of FTXUI frame focus semantics.
            ftxui::Elements content;
            content.reserve(std::max(visible_line_count, 1));

            if (lines.empty())
            {
                content.push_back(ftxui::hbox({
                    ftxui::text("1 ") | ftxui::color(ftxui::Color::GrayDark),
                    ftxui::text("<empty file>"),
                }));
            }
            else
            {
                const int first_visible_line = scroll_offset;
                const int last_visible_line  = std::min(static_cast<int>(lines.size()), first_visible_line + visible_line_count);
                const auto selected_range    = selection_bounds();

                for (int index = first_visible_line; index < last_visible_line; ++index)
                {
                    const auto rendered_line = line_with_prefix(index);

                    if (!selected_range.has_value() || index < selected_range->first.line || index > selected_range->second.line)
                    {
                        content.push_back(ftxui::text(rendered_line));
                        continue;
                    }

                    const int highlight_start = (index == selected_range->first.line) ? selected_range->first.column : 0;
                    const int highlight_end =
                        (index == selected_range->second.line) ? selected_range->second.column : static_cast<int>(rendered_line.size());
                    const int clamped_start = std::clamp(highlight_start, 0, static_cast<int>(rendered_line.size()));
                    const int clamped_end   = std::clamp(highlight_end, clamped_start, static_cast<int>(rendered_line.size()));

                    ftxui::Elements row;
                    if (clamped_start > 0)
                    {
                        row.push_back(ftxui::text(rendered_line.substr(0, static_cast<std::size_t>(clamped_start))));
                    }

                    row.push_back(ftxui::text(rendered_line.substr(
                                      static_cast<std::size_t>(clamped_start),
                                      static_cast<std::size_t>(clamped_end - clamped_start))) |
                                  ftxui::inverted);

                    if (clamped_end < static_cast<int>(rendered_line.size()))
                    {
                        row.push_back(ftxui::text(rendered_line.substr(static_cast<std::size_t>(clamped_end))));
                    }

                    content.push_back(ftxui::hbox(std::move(row)));
                }
            }

            ftxui::Element scrollbar = ftxui::text("");
            if (static_cast<int>(lines.size()) > visible_line_count)
            {
                const int total_lines = static_cast<int>(lines.size());
                const int thumb_size =
                    std::max(1, (visible_line_count * visible_line_count) / std::max(total_lines, visible_line_count));
                const int track_size = std::max(1, visible_line_count - thumb_size);
                const int max_offset = std::max(1, total_lines - visible_line_count);
                const int thumb_top  = (scroll_offset * track_size) / max_offset;

                ftxui::Elements thumb;
                thumb.reserve(visible_line_count);
                for (int row = 0; row < visible_line_count; ++row)
                {
                    const bool in_thumb = row >= thumb_top && row < (thumb_top + thumb_size);
                    thumb.push_back(ftxui::text(in_thumb ? "┃" : " "));
                }

                scrollbar = ftxui::vbox(std::move(thumb)) | ftxui::color(ftxui::Color::GrayDark);
            }

            auto log_view = ftxui::hbox({
                                ftxui::vbox(std::move(content)) | ftxui::reflect(log_viewport_box) | ftxui::flex,
                                scrollbar,
                            }) |
                            ftxui::flex;

            // The final UI is a window containing a vertical stack: file path header,
            // a separator, the scrollable log body, another separator, and the quit
            // hint at the bottom.
            return ftxui::window(ftxui::text("Slayerlog"), ftxui::vbox({
                                                               ftxui::text(
                                                                   updates_paused ? config.file_path + " [paused]" : config.file_path) |
                                                                   ftxui::bold,
                                                               ftxui::separator(),
                                                               log_view,
                                                               ftxui::separator(),
                                                               ftxui::text("q / Esc to quit"),
                                                           }));
        });

    viewer |= ftxui::CatchEvent(
        [&](ftxui::Event event)
        {
            std::lock_guard lock(lines_mutex);

            if (event == ftxui::Event::Character('q') || event == ftxui::Event::Escape)
            {
                screen.Exit();
                return true;
            }

            if (event == ftxui::Event::Custom)
            {
                return true;
            }

            if (event == ftxui::Event::Character('p'))
            {
                updates_paused = !updates_paused;
                if (!updates_paused)
                {
                    flush_paused_updates();
                    clamp_scroll_offset();
                    update_follow_bottom();
                }
                return true;
            }

            if (event == ftxui::Event::C)
            {
                return copy_to_clipboard(selection_text());
            }

            if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k'))
            {
                --scroll_offset;
                clamp_scroll_offset();
                update_follow_bottom();
                return true;
            }

            if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j'))
            {
                ++scroll_offset;
                clamp_scroll_offset();
                update_follow_bottom();
                return true;
            }

            if (event == ftxui::Event::PageUp)
            {
                scroll_offset -= visible_line_count;
                clamp_scroll_offset();
                update_follow_bottom();
                return true;
            }

            if (event == ftxui::Event::PageDown)
            {
                scroll_offset += visible_line_count;
                clamp_scroll_offset();
                update_follow_bottom();
                return true;
            }

            if (event == ftxui::Event::Home)
            {
                scroll_offset = 0;
                update_follow_bottom();
                return true;
            }

            if (event == ftxui::Event::End)
            {
                scroll_offset = max_scroll_offset();
                update_follow_bottom();
                return true;
            }

            if (event.is_mouse())
            {
                if (event.mouse().button == ftxui::Mouse::Left)
                {
                    const auto position = mouse_to_text_position(event.mouse());
                    if (event.mouse().motion == ftxui::Mouse::Pressed)
                    {
                        if (position.has_value())
                        {
                            selection_anchor      = position;
                            selection_focus       = position;
                            selection_in_progress = true;
                            return true;
                        }

                        clear_selection();
                        return false;
                    }

                    if (event.mouse().motion == ftxui::Mouse::Moved && selection_in_progress)
                    {
                        if (position.has_value())
                        {
                            selection_focus = position;
                            return true;
                        }
                    }

                    if (event.mouse().motion == ftxui::Mouse::Released)
                    {
                        selection_in_progress = false;
                        if (position.has_value() && selection_anchor.has_value())
                        {
                            selection_focus = position;
                            return true;
                        }
                    }
                }

                if (event.mouse().button == ftxui::Mouse::Right && event.mouse().motion == ftxui::Mouse::Pressed)
                {
                    return copy_to_clipboard(selection_text());
                }

                if (event.mouse().button == ftxui::Mouse::WheelUp)
                {
                    --scroll_offset;
                    clamp_scroll_offset();
                    update_follow_bottom();
                    return true;
                }

                if (event.mouse().button == ftxui::Mouse::WheelDown)
                {
                    ++scroll_offset;
                    clamp_scroll_offset();
                    update_follow_bottom();
                    return true;
                }
            }

            return false;
        });

    screen.Loop(viewer);
    keep_running = false;
    if (watcher_thread.joinable())
    {
        watcher_thread.join();
    }
    return 0;
}
