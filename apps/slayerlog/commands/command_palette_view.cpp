#include "command_palette_view.hpp"
#include "view_theme.hpp"

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
        row.push_back(ftxui::text("Enter command") | ftxui::color(theme::muted));
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

ftxui::Elements render_selectable_list(const std::vector<std::string>& items, int selected_index, const std::string& empty_message)
{
    ftxui::Elements rows;
    if (items.empty())
    {
        rows.push_back(ftxui::text(empty_message) | ftxui::color(theme::muted));
        return rows;
    }

    for (std::size_t index = 0; index < items.size(); ++index)
    {
        auto row = ftxui::text(items[index]);
        if (static_cast<int>(index) == selected_index)
        {
            row |= ftxui::inverted;
        }
        rows.push_back(std::move(row));
    }

    return rows;
}

ftxui::Elements render_filter_picker_list(const std::vector<CommandPaletteModel::FilterPickerEntry>& entries, int selected_index)
{
    ftxui::Elements rows;
    if (entries.empty())
    {
        rows.push_back(ftxui::text("No filters configured") | ftxui::color(theme::muted));
        return rows;
    }

    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        const auto& entry        = entries[index];
        const std::string prefix = entry.selected ? "[x] " : "[ ] ";
        const std::string tag    = entry.include ? "(in) " : "(out) ";
        auto row                 = ftxui::text(prefix + tag + entry.label);
        if (static_cast<int>(index) == selected_index)
        {
            row |= ftxui::inverted;
        }

        rows.push_back(std::move(row));
    }

    return rows;
}

ftxui::Elements render_command_list(const std::vector<CommandDescriptor>& commands, int selected_index)
{
    ftxui::Elements rows;
    if (commands.empty())
    {
        rows.push_back(ftxui::text("No matching commands") | ftxui::color(theme::muted));
        return rows;
    }

    for (std::size_t index = 0; index < commands.size(); ++index)
    {
        const auto& command = commands[index];
        auto row            = ftxui::vbox({
            ftxui::text(command.name + " - " + command.summary),
            ftxui::text(command.usage) | ftxui::color(theme::muted),
        });

        if (static_cast<int>(index) == selected_index)
        {
            row |= ftxui::inverted;
        }

        rows.push_back(std::move(row));
    }

    return rows;
}

ftxui::Element build_palette_help(CommandPaletteMode mode)
{
    auto sep = []() { return ftxui::text("  "); };

    if (mode == CommandPaletteMode::History)
    {
        return ftxui::hbox({
            theme::key_hint("Enter", "execute"),
            sep(),
            theme::key_hint("Tab", "copy to input"),
            sep(),
            theme::key_hint("Ctrl+R", "commands"),
            sep(),
            theme::key_hint("Esc", "close"),
        });
    }

    if (mode == CommandPaletteMode::CloseOpenFile)
    {
        return ftxui::hbox({
            theme::key_hint("\xe2\x86\x91\xe2\x86\x93", "select"),
            sep(),
            theme::key_hint("Enter", "close file"),
            sep(),
            theme::key_hint("Esc", "cancel"),
        });
    }

    if (mode == CommandPaletteMode::DeleteFilters)
    {
        return ftxui::hbox({
            theme::key_hint("\xe2\x86\x91\xe2\x86\x93", "select"),
            sep(),
            theme::key_hint("Space", "toggle"),
            sep(),
            theme::key_hint("Enter", "delete marked"),
            sep(),
            theme::key_hint("Esc", "cancel"),
        });
    }

    return ftxui::hbox({
        theme::key_hint("Tab", "complete"),
        sep(),
        theme::key_hint("Enter", "execute"),
        sep(),
        theme::key_hint("Ctrl+R", "history"),
        sep(),
        theme::key_hint("Esc", "close"),
    });
}

} // namespace

ftxui::Element CommandPaletteView::render(const CommandPaletteModel& command_palette) const
{
    ftxui::Elements command_rows;
    if (command_palette.mode == CommandPaletteMode::History)
    {
        const std::string empty_msg = command_palette.query.empty() ? "No previously run commands" : "No matching history commands";
        command_rows                = render_selectable_list(command_palette.matching_history_entries, command_palette.selected_index, empty_msg);
    }
    else if (command_palette.mode == CommandPaletteMode::CloseOpenFile)
    {
        command_rows = render_selectable_list(command_palette.open_files, command_palette.selected_index, "No open files");
    }
    else if (command_palette.mode == CommandPaletteMode::DeleteFilters)
    {
        command_rows = render_filter_picker_list(command_palette.filter_picker_entries, command_palette.selected_index);
    }
    else
    {
        command_rows = render_command_list(command_palette.matching_commands, command_palette.selected_index);
    }

    ftxui::Element status = build_palette_help(command_palette.mode);
    if (!command_palette.status_message.empty())
    {
        status = ftxui::text(command_palette.status_message) | ftxui::color(command_palette.status_is_error ? theme::error_fg : theme::success_fg);
    }

    const std::string title = command_palette.mode == CommandPaletteMode::History         ? "Command History"
                              : command_palette.mode == CommandPaletteMode::CloseOpenFile ? "Close Open File"
                              : command_palette.mode == CommandPaletteMode::DeleteFilters ? "Delete Filters"
                                                                                          : "Command Palette";

    ftxui::Elements body;
    if (command_palette.mode == CommandPaletteMode::CloseOpenFile)
    {
        body.push_back(ftxui::text("Select file to close") | ftxui::color(theme::muted));
    }
    else if (command_palette.mode == CommandPaletteMode::DeleteFilters)
    {
        body.push_back(ftxui::text("Mark filters to delete") | ftxui::color(theme::muted));
    }
    else
    {
        body.push_back(render_command_palette_query(command_palette));
    }
    body.push_back(ftxui::separator());
    body.push_back(ftxui::vbox(std::move(command_rows)));
    body.push_back(ftxui::separator());
    body.push_back(status);

    return ftxui::center(ftxui::clear_under(ftxui::window(ftxui::text(title), ftxui::vbox(std::move(body))) | ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 80)));
}

} // namespace slayerlog
