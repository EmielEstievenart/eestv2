#include "log_controller.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>

#ifdef _WIN32
#    define NOMINMAX
#    include <windows.h>
#endif

namespace slayerlog
{

namespace
{

bool is_before(const TextPosition& lhs, const TextPosition& rhs)
{
    return lhs.line < rhs.line || (lhs.line == rhs.line && lhs.column < rhs.column);
}

#ifndef _WIN32

bool env_var_is_set(const char* name)
{
    const auto* value = std::getenv(name);
    return value != nullptr && value[0] != '\0';
}

bool is_ssh_session()
{
    return env_var_is_set("SSH_CONNECTION") || env_var_is_set("SSH_CLIENT") || env_var_is_set("SSH_TTY");
}

bool write_text_to_command(const char* command, const std::string& text)
{
    auto* pipe = popen(command, "w");
    if (pipe == nullptr)
    {
        return false;
    }

    const std::size_t bytes_written = std::fwrite(text.data(), 1, text.size(), pipe);
    const int status                = pclose(pipe);
    return bytes_written == text.size() && status == 0;
}

std::string base64_encode(const std::string& text)
{
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    encoded.reserve(((text.size() + 2) / 3) * 4);

    std::size_t index = 0;
    while (index + 3 <= text.size())
    {
        const unsigned int value = (static_cast<unsigned char>(text[index]) << 16) | (static_cast<unsigned char>(text[index + 1]) << 8) | static_cast<unsigned char>(text[index + 2]);
        encoded.push_back(alphabet[(value >> 18) & 0x3F]);
        encoded.push_back(alphabet[(value >> 12) & 0x3F]);
        encoded.push_back(alphabet[(value >> 6) & 0x3F]);
        encoded.push_back(alphabet[value & 0x3F]);
        index += 3;
    }

    const std::size_t remainder = text.size() - index;
    if (remainder == 1)
    {
        const unsigned int value = static_cast<unsigned char>(text[index]) << 16;
        encoded.push_back(alphabet[(value >> 18) & 0x3F]);
        encoded.push_back(alphabet[(value >> 12) & 0x3F]);
        encoded.push_back('=');
        encoded.push_back('=');
    }
    else if (remainder == 2)
    {
        const unsigned int value = (static_cast<unsigned char>(text[index]) << 16) | (static_cast<unsigned char>(text[index + 1]) << 8);
        encoded.push_back(alphabet[(value >> 18) & 0x3F]);
        encoded.push_back(alphabet[(value >> 12) & 0x3F]);
        encoded.push_back(alphabet[(value >> 6) & 0x3F]);
        encoded.push_back('=');
    }

    return encoded;
}

std::string build_osc52_sequence(const std::string& text)
{
    const std::string payload = "52;c;" + base64_encode(text) + '\a';

    if (env_var_is_set("TMUX"))
    {
        return "\x1bPtmux;\x1b]" + payload + "\x1b\\";
    }

    const auto* term = std::getenv("TERM");
    if (term != nullptr && std::string(term).rfind("screen", 0) == 0)
    {
        return "\x1bP\x1b]" + payload + "\x1b\\";
    }

    return "\x1b]" + payload;
}

bool write_text_to_terminal_clipboard(const std::string& text)
{
    auto* terminal = std::fopen("/dev/tty", "w");
    if (terminal == nullptr)
    {
        return false;
    }

    const auto sequence      = build_osc52_sequence(text);
    const auto bytes_written = std::fwrite(sequence.data(), 1, sequence.size(), terminal);
    const bool flushed       = std::fflush(terminal) == 0;
    std::fclose(terminal);

    return bytes_written == sequence.size() && flushed;
}

bool copy_with_local_clipboard_tools(const std::string& text)
{
#    ifdef __APPLE__
    if (write_text_to_command("pbcopy 2>/dev/null", text))
    {
        return true;
    }
#    endif

    if (write_text_to_command("wl-copy --type text/plain;charset=utf-8 2>/dev/null", text))
    {
        return true;
    }

    if (write_text_to_command("xclip -in -selection clipboard 2>/dev/null", text))
    {
        return true;
    }

    if (write_text_to_command("xsel --clipboard --input 2>/dev/null", text))
    {
        return true;
    }

    return false;
}

bool copy_text_to_clipboard_on_unix(const std::string& text)
{
    if (is_ssh_session())
    {
        return write_text_to_terminal_clipboard(text) || copy_with_local_clipboard_tools(text);
    }

    return copy_with_local_clipboard_tools(text) || write_text_to_terminal_clipboard(text);
}

#endif

} // namespace

void LogController::reset()
{
    _first_visible_line_index = VisibleLineIndex {0};
    _first_visible_col        = 0;
    _follow_bottom            = true;

    _active_find_entry_index.reset();

    _selection_in_progress = false;
    _selection_anchor.reset();
    _selection_focus.reset();
}

int LogController::first_visible_col(const LogModel& model, int viewport_col_count) const
{
    return std::clamp(_first_visible_col, 0, max_first_visible_col(model, viewport_col_count));
}

VisibleLineIndex LogController::first_visible_line_index(const LogModel& model, int viewport_line_count) const
{
    if (_follow_bottom)
    {
        return VisibleLineIndex {max_first_visible_line_index(model, viewport_line_count)};
    }

    return VisibleLineIndex {std::clamp(_first_visible_line_index.value, 0, max_first_visible_line_index(model, viewport_line_count))};
}

void LogController::scroll_up(const LogModel& model, int viewport_line_count, int amount)
{
    _first_visible_line_index = VisibleLineIndex {std::max(0, first_visible_line_index(model, viewport_line_count).value - std::max(1, amount))};
    _follow_bottom            = _first_visible_line_index.value >= max_first_visible_line_index(model, viewport_line_count);
}

void LogController::scroll_down(const LogModel& model, int viewport_line_count, int amount)
{
    _first_visible_line_index = VisibleLineIndex {std::min(first_visible_line_index(model, viewport_line_count).value + std::max(1, amount), max_first_visible_line_index(model, viewport_line_count))};
    _follow_bottom            = _first_visible_line_index.value >= max_first_visible_line_index(model, viewport_line_count);
}

void LogController::scroll_left(int amount)
{
    _first_visible_col = std::max(0, _first_visible_col - std::max(1, amount));
}

void LogController::scroll_right(const LogModel& model, int viewport_col_count, int amount)
{
    _first_visible_col = std::min(max_first_visible_col(model, viewport_col_count), _first_visible_col + std::max(1, amount));
}

void LogController::scroll_to_top(const LogModel& model, int viewport_line_count)
{
    _first_visible_line_index = VisibleLineIndex {0};
    _follow_bottom            = _first_visible_line_index.value >= max_first_visible_line_index(model, viewport_line_count);
}

void LogController::scroll_to_bottom()
{
    _follow_bottom = true;
}

bool LogController::go_to_line(const LogModel& model, int line_number, int viewport_line_count)
{
    const auto target_visible_index = model.visible_line_index_for_line_number(line_number);
    if (!target_visible_index.has_value())
    {
        return false;
    }

    center_on_visible_line(model, *target_visible_index, viewport_line_count);
    return true;
}

bool LogController::set_find_query(LogModel& model, std::string query, int viewport_line_count)
{
    const bool has_matches = model.set_find_query(std::move(query));
    _active_find_entry_index.reset();
    if (!has_matches)
    {
        return false;
    }

    return go_to_next_find_match(model, viewport_line_count);
}

void LogController::clear_find(LogModel& model)
{
    model.clear_find_query();
    _active_find_entry_index.reset();
}

bool LogController::go_to_next_find_match(const LogModel& model, int viewport_line_count)
{
    if (!model.find_active() || model.total_find_match_count() == 0)
    {
        return false;
    }

    int current_position = -1;
    if (_active_find_entry_index.has_value())
    {
        const auto position = model.find_match_position_for_entry_index(*_active_find_entry_index);
        if (position.has_value())
        {
            current_position = position->value;
        }
    }

    for (int offset = 1; offset <= model.total_find_match_count(); ++offset)
    {
        const int next_position = (current_position + offset) % model.total_find_match_count();
        const auto entry_index  = model.find_match_entry_index(FindResultIndex {next_position});
        if (!entry_index.has_value() || !model.entry_index_is_visible(*entry_index))
        {
            continue;
        }

        _active_find_entry_index = *entry_index;
        const auto visible_index = model.visible_line_index_for_entry(*entry_index);
        if (!visible_index.has_value())
        {
            return false;
        }

        center_on_visible_line(model, *visible_index, viewport_line_count);
        return true;
    }

    return false;
}

bool LogController::go_to_previous_find_match(const LogModel& model, int viewport_line_count)
{
    if (!model.find_active() || model.total_find_match_count() == 0)
    {
        return false;
    }

    int current_position = 0;
    if (_active_find_entry_index.has_value())
    {
        const auto position = model.find_match_position_for_entry_index(*_active_find_entry_index);
        if (position.has_value())
        {
            current_position = position->value;
        }
    }

    for (int offset = 1; offset <= model.total_find_match_count(); ++offset)
    {
        const int previous_position = (current_position - offset + model.total_find_match_count()) % model.total_find_match_count();
        const auto entry_index      = model.find_match_entry_index(FindResultIndex {previous_position});
        if (!entry_index.has_value() || !model.entry_index_is_visible(*entry_index))
        {
            continue;
        }

        _active_find_entry_index = *entry_index;
        const auto visible_index = model.visible_line_index_for_entry(*entry_index);
        if (!visible_index.has_value())
        {
            return false;
        }

        center_on_visible_line(model, *visible_index, viewport_line_count);
        return true;
    }

    return false;
}

std::optional<VisibleLineIndex> LogController::active_find_visible_index(const LogModel& model) const
{
    if (!_active_find_entry_index.has_value())
    {
        return std::nullopt;
    }

    return model.visible_line_index_for_entry(*_active_find_entry_index);
}

void LogController::begin_selection(const LogModel& model, TextPosition position)
{
    _selection_anchor      = clamp_selection_position(model, position);
    _selection_focus       = _selection_anchor;
    _selection_in_progress = _selection_anchor.has_value();
}

void LogController::update_selection(const LogModel& model, TextPosition position)
{
    if (!_selection_in_progress || !_selection_anchor.has_value())
    {
        return;
    }

    _selection_focus = clamp_selection_position(model, position);
}

void LogController::end_selection(const LogModel& model, std::optional<TextPosition> position)
{
    _selection_in_progress = false;
    if (position.has_value() && _selection_anchor.has_value())
    {
        _selection_focus = clamp_selection_position(model, *position);
    }
}

void LogController::clear_selection()
{
    _selection_anchor.reset();
    _selection_focus.reset();
    _selection_in_progress = false;
}

bool LogController::selection_in_progress() const
{
    return _selection_in_progress;
}

std::optional<std::pair<TextPosition, TextPosition>> LogController::selection_bounds(const LogModel& model) const
{
    if (!_selection_anchor.has_value() || !_selection_focus.has_value() || model.line_count() == 0)
    {
        return std::nullopt;
    }

    auto start = clamp_selection_position(model, *_selection_anchor);
    auto end   = clamp_selection_position(model, *_selection_focus);
    if (is_before(end, start))
    {
        std::swap(start, end);
    }

    return std::pair(start, end);
}

std::string LogController::selection_text(const LogModel& model) const
{
    const auto bounds = selection_bounds(model);
    if (!bounds.has_value())
    {
        return {};
    }

    const auto [start, end] = *bounds;
    std::ostringstream output;
    for (int line_index = start.line; line_index <= end.line; ++line_index)
    {
        const auto line            = model.rendered_line(line_index);
        const int line_start       = (line_index == start.line) ? start.column : 0;
        const int line_end         = (line_index == end.line) ? end.column : static_cast<int>(line.size());
        const int clamped_start    = std::clamp(line_start, 0, static_cast<int>(line.size()));
        const int clamped_end      = std::clamp(line_end, clamped_start, static_cast<int>(line.size()));
        const auto selection_count = static_cast<std::size_t>(clamped_end - clamped_start);

        output << line.substr(static_cast<std::size_t>(clamped_start), selection_count);
        if (line_index != end.line)
        {
            output << '\n';
        }
    }

    return output.str();
}

LogEventResult LogController::handle_event(LogModel& model, ftxui::Event event, int viewport_line_count, int viewport_col_count, const std::function<std::optional<TextPosition>(const ftxui::Mouse& mouse)>& mouse_to_text_position)
{
    const int fast_horizontal_step = std::max(1, (viewport_col_count - 1) / 2);
    if (event == ftxui::Event::Escape)
    {
        if (model.find_active())
        {
            clear_find(model);
            return {true, false};
        }

        return {true, true};
    }

    if (event == ftxui::Event::Character('q'))
    {
        return {true, true};
    }

    if (event == ftxui::Event::Custom)
    {
        return {true, false};
    }

    if (event == ftxui::Event::Character('p'))
    {
        model.toggle_pause();
        return {true, false};
    }

    if (model.find_active() && event == ftxui::Event::ArrowRight)
    {
        return {go_to_next_find_match(model, viewport_line_count), false};
    }

    if (model.find_active() && event == ftxui::Event::ArrowLeft)
    {
        return {go_to_previous_find_match(model, viewport_line_count), false};
    }

    if (event == ftxui::Event::ArrowLeft)
    {
        scroll_left();
        return {true, false};
    }

    if (event == ftxui::Event::ArrowLeftCtrl)
    {
        scroll_left(fast_horizontal_step);
        return {true, false};
    }

    if (event == ftxui::Event::ArrowRight)
    {
        scroll_right(model, viewport_col_count);
        return {true, false};
    }

    if (event == ftxui::Event::ArrowRightCtrl)
    {
        scroll_right(model, viewport_col_count, fast_horizontal_step);
        return {true, false};
    }

    if (event == ftxui::Event::C)
    {
        return {copy_selection_to_clipboard(model), false};
    }

    if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k'))
    {
        scroll_up(model, viewport_line_count);
        return {true, false};
    }

    if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j'))
    {
        scroll_down(model, viewport_line_count);
        return {true, false};
    }

    if (event == ftxui::Event::PageUp)
    {
        scroll_up(model, viewport_line_count, viewport_line_count);
        return {true, false};
    }

    if (event == ftxui::Event::PageDown)
    {
        scroll_down(model, viewport_line_count, viewport_line_count);
        return {true, false};
    }

    if (event == ftxui::Event::Home)
    {
        scroll_to_top(model, viewport_line_count);
        return {true, false};
    }

    if (event == ftxui::Event::End)
    {
        scroll_to_bottom();
        return {true, false};
    }

    if (event.is_mouse())
    {
        if (!mouse_to_text_position)
        {
            return {false, false};
        }

        if (event.mouse().button == ftxui::Mouse::Left)
        {
            const auto position = mouse_to_text_position(event.mouse());
            if (event.mouse().motion == ftxui::Mouse::Pressed)
            {
                if (position.has_value())
                {
                    begin_selection(model, *position);
                    return {true, false};
                }

                clear_selection();
                return {false, false};
            }

            if (event.mouse().motion == ftxui::Mouse::Moved && selection_in_progress())
            {
                if (position.has_value())
                {
                    update_selection(model, *position);
                    return {true, false};
                }
            }

            if (event.mouse().motion == ftxui::Mouse::Released)
            {
                end_selection(model, position);
                return {position.has_value(), false};
            }
        }

        if (event.mouse().button == ftxui::Mouse::Right && event.mouse().motion == ftxui::Mouse::Pressed)
        {
            return {copy_selection_to_clipboard(model), false};
        }

        if (event.mouse().button == ftxui::Mouse::WheelUp)
        {
            scroll_up(model, viewport_line_count);
            return {true, false};
        }

        if (event.mouse().button == ftxui::Mouse::WheelDown)
        {
            scroll_down(model, viewport_line_count);
            return {true, false};
        }
    }

    return {false, false};
}

bool LogController::copy_selection_to_clipboard(const LogModel& model) const
{
    const auto text = selection_text(model);
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
    return copy_text_to_clipboard_on_unix(text);
#endif
}

int LogController::max_first_visible_line_index(const LogModel& model, int viewport_line_count) const
{
    return std::max(0, model.line_count() - std::max(1, viewport_line_count));
}

int LogController::max_first_visible_col(const LogModel& model, int viewport_col_count) const
{
    return std::max(0, model.max_rendered_line_width() - std::max(1, viewport_col_count));
}

void LogController::center_on_visible_line(const LogModel& model, VisibleLineIndex target_visible_index, int viewport_line_count)
{
    _first_visible_line_index       = VisibleLineIndex {target_visible_index.value - (std::max(1, viewport_line_count) / 2)};
    _first_visible_line_index.value = std::clamp(_first_visible_line_index.value, 0, max_first_visible_line_index(model, viewport_line_count));
    _follow_bottom                  = _first_visible_line_index.value >= max_first_visible_line_index(model, viewport_line_count);
}

TextPosition LogController::clamp_selection_position(const LogModel& model, TextPosition position) const
{
    if (model.line_count() == 0)
    {
        return TextPosition {0, 0};
    }

    position.line          = std::clamp(position.line, 0, model.line_count() - 1);
    const auto line_length = static_cast<int>(model.rendered_line(position.line).size());
    position.column        = std::clamp(position.column, 0, line_length);
    return position;
}

} // namespace slayerlog
