#include "command_palette_view.hpp"

#include <algorithm>
#include <string>

namespace slayerlog
{

namespace
{

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
        row.push_back(ftxui::text("Enter command") | ftxui::color(ftxui::Color::GrayDark));
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

    return ftxui::hbox(std::move(row)) | ftxui::focusPosition(static_cast<int>(cursor_position) + 2, 0) | ftxui::xframe;
}

} // namespace

ftxui::Element CommandPaletteView::render(const CommandPaletteModel& command_palette) const
{
    ftxui::Elements command_rows;
    if (command_palette.mode == CommandPaletteMode::History)
    {
        if (command_palette.matching_history_entries.empty())
        {
            command_rows.push_back(
                ftxui::text(command_palette.query.empty() ? "No previously run commands" : "No matching history commands") |
                ftxui::color(ftxui::Color::GrayDark));
        }
        else
        {
            for (std::size_t index = 0; index < command_palette.matching_history_entries.size(); ++index)
            {
                auto row = ftxui::text(command_palette.matching_history_entries[index]);
                if (static_cast<int>(index) == command_palette.selected_index)
                {
                    row |= ftxui::inverted;
                }

                command_rows.push_back(std::move(row));
            }
        }
    }
    else if (command_palette.mode == CommandPaletteMode::CloseOpenFile)
    {
        if (command_palette.open_files.empty())
        {
            command_rows.push_back(ftxui::text("No open files") | ftxui::color(ftxui::Color::GrayDark));
        }
        else
        {
            for (std::size_t index = 0; index < command_palette.open_files.size(); ++index)
            {
                auto row = ftxui::text(command_palette.open_files[index]);
                if (static_cast<int>(index) == command_palette.selected_index)
                {
                    row |= ftxui::inverted;
                }

                command_rows.push_back(std::move(row));
            }
        }
    }
    else
    {
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
    }

    const std::string help_text =
        command_palette.mode == CommandPaletteMode::History
            ? "Enter executes selected history command. Tab copies to input for editing. Ctrl+R toggles commands. Esc closes."
        : command_palette.mode == CommandPaletteMode::CloseOpenFile
            ? "Arrow keys select an open file. Enter closes selected file. Esc cancels."
            : "Tab completes selected command. Enter executes input. Ctrl+R toggles history. Esc closes.";

    ftxui::Element status = ftxui::text(help_text) | ftxui::color(ftxui::Color::GrayDark);
    if (!command_palette.status_message.empty())
    {
        status = ftxui::text(command_palette.status_message) |
                 ftxui::color(command_palette.status_is_error ? ftxui::Color::Red : ftxui::Color::GreenLight);
    }

    const std::string title = command_palette.mode == CommandPaletteMode::History         ? "Command History"
                              : command_palette.mode == CommandPaletteMode::CloseOpenFile ? "Close Open File"
                                                                                          : "Command Palette";

    ftxui::Elements body;
    if (command_palette.mode == CommandPaletteMode::CloseOpenFile)
    {
        body.push_back(ftxui::text("Select file to close") | ftxui::color(ftxui::Color::GrayDark));
    }
    else
    {
        body.push_back(render_command_palette_query(command_palette));
    }
    body.push_back(ftxui::separator());
    body.push_back(ftxui::vbox(std::move(command_rows)));
    body.push_back(ftxui::separator());
    body.push_back(status);

    return ftxui::center(ftxui::clear_under(ftxui::window(ftxui::text(title), ftxui::vbox(std::move(body))) |
                                            ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 80)));
}

} // namespace slayerlog
