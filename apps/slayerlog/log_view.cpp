#include "log_view.hpp"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <utility>

namespace slayerlog
{

namespace
{

constexpr int window_chrome_height = 7;

int estimate_visible_line_count(const ftxui::Box& viewport_box, int screen_height)
{
    if (viewport_box.y_max > viewport_box.y_min)
    {
        return viewport_box.y_max - viewport_box.y_min + 1;
    }

    return std::max(1, screen_height - window_chrome_height);
}

std::string build_filter_status_text(const LogViewModel& model)
{
    std::ostringstream output;
    if (model.include_filters().empty() && model.exclude_filters().empty())
    {
        output << "Filters: none";
        return output.str();
    }

    output << "Filters:";
    if (!model.include_filters().empty())
    {
        output << " in(";
        for (std::size_t index = 0; index < model.include_filters().size(); ++index)
        {
            if (index > 0)
            {
                output << ", ";
            }

            output << model.include_filters()[index];
        }

        output << ")";
    }

    if (!model.exclude_filters().empty())
    {
        output << " out(";
        for (std::size_t index = 0; index < model.exclude_filters().size(); ++index)
        {
            if (index > 0)
            {
                output << ", ";
            }

            output << model.exclude_filters()[index];
        }

        output << ")";
    }

    return output.str();
}

ftxui::Element render_command_palette_query(const CommandPaletteModel& command_palette)
{
    const std::size_t cursor_position = std::min(command_palette.cursor_position, command_palette.query.size());
    const std::string prefix          = command_palette.query.substr(0, cursor_position);
    const bool cursor_at_end          = cursor_position >= command_palette.query.size();
    const std::string cursor_text     = cursor_at_end ? " " : command_palette.query.substr(cursor_position, 1);
    const std::string suffix          = cursor_at_end ? std::string() : command_palette.query.substr(cursor_position + 1);

    ftxui::Elements row;
    row.push_back(ftxui::text("> ") | ftxui::bold);
    if (command_palette.query.empty())
    {
        row.push_back(ftxui::text("Type a command") | ftxui::color(ftxui::Color::GrayDark));
        row.push_back(ftxui::text(" ") | ftxui::inverted);
        return ftxui::hbox(std::move(row));
    }

    if (!prefix.empty())
    {
        row.push_back(ftxui::text(prefix));
    }

    row.push_back(ftxui::text(cursor_text) | ftxui::inverted);

    if (!suffix.empty())
    {
        row.push_back(ftxui::text(suffix));
    }

    return ftxui::hbox(std::move(row));
}

ftxui::Element render_command_palette(const CommandPaletteModel& command_palette)
{
    ftxui::Elements command_rows;
    if (command_palette.matching_commands.empty())
    {
        command_rows.push_back(ftxui::text("No matching commands") | ftxui::color(ftxui::Color::GrayDark));
    }
    else
    {
        for (std::size_t index = 0; index < command_palette.matching_commands.size(); ++index)
        {
            const auto& command = command_palette.matching_commands[index];
            auto row            = ftxui::vbox({
                ftxui::text(command.name + " - " + command.summary),
                ftxui::text(command.usage) | ftxui::color(ftxui::Color::GrayDark),
            });

            if (static_cast<int>(index) == command_palette.selected_index)
            {
                row |= ftxui::inverted;
            }

            command_rows.push_back(row);
        }
    }

    ftxui::Element status =
        ftxui::text("Tab completes selected command. Enter executes. Esc closes.") | ftxui::color(ftxui::Color::GrayDark);
    if (!command_palette.status_message.empty())
    {
        status = ftxui::text(command_palette.status_message) |
                 ftxui::color(command_palette.status_is_error ? ftxui::Color::Red : ftxui::Color::GreenLight);
    }

    return ftxui::center(ftxui::clear_under(ftxui::window(ftxui::text("Command Palette"), ftxui::vbox({
                                                                                              render_command_palette_query(command_palette),
                                                                                              ftxui::separator(),
                                                                                              ftxui::vbox(std::move(command_rows)),
                                                                                              ftxui::separator(),
                                                                                              status,
                                                                                          })) |
                                            ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 80)));
}

} // namespace

ftxui::Element LogView::render(LogViewModel& model, const std::string& header_text, int screen_height,
                               const CommandPaletteModel& command_palette)
{
    model.set_visible_line_count(estimate_visible_line_count(_viewport_box, screen_height));

    ftxui::Elements content;
    content.reserve(static_cast<std::size_t>(std::max(1, model.visible_line_count())));

    if (model.line_count() == 0)
    {
        content.push_back(ftxui::hbox({
            ftxui::text("1 ") | ftxui::color(ftxui::Color::GrayDark),
            ftxui::text(model.total_line_count() == 0 ? "<empty file>" : "<no matching lines>"),
        }));
    }
    else
    {
        const int first_visible_line = model.scroll_offset();
        const int last_visible_line  = std::min(model.line_count(), first_visible_line + model.visible_line_count());
        const auto selected_range    = model.selection_bounds();

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

            row.push_back(ftxui::text(rendered_line.substr(static_cast<std::size_t>(clamped_start),
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

    ftxui::Element base_view = ftxui::window(
        ftxui::text("Slayerlog"), ftxui::vbox({
                                      ftxui::text(model.updates_paused() ? header_text + " [paused]" : header_text) | ftxui::bold,
                                      ftxui::separator(),
                                      log_view,
                                      ftxui::separator(),
                                      ftxui::text(build_filter_status_text(model)) | ftxui::color(ftxui::Color::GrayDark),
                                      ftxui::text("Ctrl+P commands | p pause | q / Esc quit") | ftxui::color(ftxui::Color::GrayDark),
                                  }));

    if (!command_palette.open)
    {
        return base_view;
    }

    return ftxui::dbox({
        base_view,
        render_command_palette(command_palette),
    });
}

std::optional<TextPosition> LogView::mouse_to_text_position(const LogViewModel& model, const ftxui::Mouse& mouse) const
{
    if (model.line_count() == 0)
    {
        return std::nullopt;
    }

    if (mouse.x < _viewport_box.x_min || mouse.x > _viewport_box.x_max || mouse.y < _viewport_box.y_min || mouse.y > _viewport_box.y_max)
    {
        return std::nullopt;
    }

    const int line_index = model.scroll_offset() + (mouse.y - _viewport_box.y_min);
    if (line_index < 0 || line_index >= model.line_count())
    {
        return std::nullopt;
    }

    const auto line = model.rendered_line(line_index);
    return TextPosition {
        line_index,
        std::clamp(mouse.x - _viewport_box.x_min, 0, static_cast<int>(line.size())),
    };
}

} // namespace slayerlog
