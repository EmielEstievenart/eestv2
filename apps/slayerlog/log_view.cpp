#include "log_view.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace slayerlog
{

namespace
{

constexpr int window_chrome_height = 6;

int estimate_visible_line_count(const ftxui::Box& viewport_box, int screen_height)
{
    if (viewport_box.y_max > viewport_box.y_min)
    {
        return viewport_box.y_max - viewport_box.y_min + 1;
    }

    return std::max(1, screen_height - window_chrome_height);
}

} // namespace

ftxui::Element LogView::render(LogViewModel& model, const std::string& header_text, int screen_height)
{
    model.set_visible_line_count(estimate_visible_line_count(_viewport_box, screen_height));

    ftxui::Elements content;
    content.reserve(static_cast<std::size_t>(std::max(1, model.visible_line_count())));

    if (model.line_count() == 0)
    {
        content.push_back(ftxui::hbox({
            ftxui::text("1 ") | ftxui::color(ftxui::Color::GrayDark),
            ftxui::text("<empty file>"),
        }));
    }
    else
    {
        const int first_visible_line = model.scroll_offset();
        const int last_visible_line =
            std::min(model.line_count(), first_visible_line + model.visible_line_count());
        const auto selected_range = model.selection_bounds();

        for (int index = first_visible_line; index < last_visible_line; ++index)
        {
            const auto rendered_line = model.rendered_line(index);
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
    if (model.line_count() > model.visible_line_count())
    {
        const int total_lines = model.line_count();
        const int thumb_size =
            std::max(1, (model.visible_line_count() * model.visible_line_count()) / std::max(total_lines, model.visible_line_count()));
        const int track_size = std::max(1, model.visible_line_count() - thumb_size);
        const int max_offset = std::max(1, total_lines - model.visible_line_count());
        const int thumb_top  = (model.scroll_offset() * track_size) / max_offset;

        ftxui::Elements thumb;
        thumb.reserve(static_cast<std::size_t>(model.visible_line_count()));
        for (int row = 0; row < model.visible_line_count(); ++row)
        {
            thumb.push_back(ftxui::text(row >= thumb_top && row < (thumb_top + thumb_size) ? "┃" : " "));
        }

        scrollbar = ftxui::vbox(std::move(thumb)) | ftxui::color(ftxui::Color::GrayDark);
    }

    auto log_view = ftxui::hbox({
                        ftxui::vbox(std::move(content)) | ftxui::reflect(_viewport_box) | ftxui::flex,
                        scrollbar,
                    }) |
                    ftxui::flex;

    return ftxui::window(ftxui::text("Slayerlog"), ftxui::vbox({
                                                          ftxui::text(
                                                              model.updates_paused() ? header_text + " [paused]" : header_text) |
                                                              ftxui::bold,
                                                          ftxui::separator(),
                                                          log_view,
                                                          ftxui::separator(),
                                                          ftxui::text("q / Esc to quit"),
                                                      }));
}

std::optional<TextPosition> LogView::mouse_to_text_position(const LogViewModel& model, const ftxui::Mouse& mouse) const
{
    if (model.line_count() == 0)
    {
        return std::nullopt;
    }

    if (mouse.x < _viewport_box.x_min || mouse.x > _viewport_box.x_max || mouse.y < _viewport_box.y_min ||
        mouse.y > _viewport_box.y_max)
    {
        return std::nullopt;
    }

    const int line_index = model.scroll_offset() + (mouse.y - _viewport_box.y_min);
    if (line_index < 0 || line_index >= model.line_count())
    {
        return std::nullopt;
    }

    const auto line = model.rendered_line(line_index);
    return TextPosition{
        line_index,
        std::clamp(mouse.x - _viewport_box.x_min, 0, static_cast<int>(line.size())),
    };
}

} // namespace slayerlog
