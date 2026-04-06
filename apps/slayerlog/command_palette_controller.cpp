#include "command_palette_controller.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace slayerlog
{

namespace
{

bool is_utf8_continuation_byte(unsigned char value)
{
    return (value & 0xC0U) == 0x80U;
}

std::size_t previous_codepoint_start(const std::string& text, std::size_t cursor_position)
{
    if (cursor_position == 0)
    {
        return 0;
    }

    std::size_t position = cursor_position - 1;
    while (position > 0 && is_utf8_continuation_byte(static_cast<unsigned char>(text[position])))
    {
        --position;
    }

    return position;
}

std::size_t next_codepoint_end(const std::string& text, std::size_t cursor_position)
{
    if (cursor_position >= text.size())
    {
        return text.size();
    }

    std::size_t position = cursor_position + 1;
    while (position < text.size() && is_utf8_continuation_byte(static_cast<unsigned char>(text[position])))
    {
        ++position;
    }

    return position;
}

std::string command_arguments_from_query(std::string_view query)
{
    const std::size_t separator_index = query.find_first_of(" \t\r\n");
    if (separator_index == std::string_view::npos)
    {
        return {};
    }

    std::string arguments(query.substr(separator_index + 1));
    const std::size_t first_argument = arguments.find_first_not_of(" \t\r\n");
    if (first_argument == std::string::npos)
    {
        return {};
    }

    arguments.erase(0, first_argument);
    return arguments;
}

std::pair<std::size_t, std::size_t> command_name_range(std::string_view query)
{
    const std::size_t command_start = query.find_first_not_of(" \t\r\n");
    if (command_start == std::string_view::npos)
    {
        return {query.size(), query.size()};
    }

    const std::size_t command_end = query.find_first_of(" \t\r\n", command_start);
    if (command_end == std::string_view::npos)
    {
        return {command_start, query.size()};
    }

    return {command_start, command_end};
}

} // namespace

CommandPaletteController::CommandPaletteController(CommandPaletteModel& model, CommandManager& command_manager)
    : _model(model), _command_manager(command_manager)
{
    refresh_matches();
}

CommandPaletteController::CommandPaletteController(CommandPaletteModel& model, CommandManager& command_manager,
                                                   CommandHistory& command_history)
    : _model(model), _command_manager(command_manager), _command_history(&command_history)
{
    refresh_matches();
}

bool CommandPaletteController::is_open() const
{
    return _model.open;
}

const CommandPaletteModel& CommandPaletteController::model() const
{
    return _model;
}

void CommandPaletteController::open()
{
    _model.open = true;
    _model.mode = CommandPaletteMode::Commands;
    _model.query.clear();
    _model.open_files.clear();
    _close_open_file_selection_handler = {};
    _model.cursor_position             = 0;
    _model.selected_index              = 0;
    _model.status_message.clear();
    _model.status_is_error = false;
    refresh_matches();
}

void CommandPaletteController::open_close_open_file_picker(std::vector<std::string> open_files,
                                                           std::function<CommandResult(std::size_t selected_index)> on_confirm)
{
    _model.open = true;
    _model.mode = CommandPaletteMode::CloseOpenFile;
    _model.query.clear();
    _model.open_files                  = std::move(open_files);
    _close_open_file_selection_handler = std::move(on_confirm);
    _model.cursor_position             = 0;
    _model.selected_index              = 0;
    _model.status_message.clear();
    _model.status_is_error = false;
    refresh_matches();
}

void CommandPaletteController::close()
{
    _model.open = false;
    _model.mode = CommandPaletteMode::Commands;
    _model.query.clear();
    _model.open_files.clear();
    _close_open_file_selection_handler = {};
    _model.cursor_position             = 0;
    _model.selected_index              = 0;
    refresh_matches();
}

bool CommandPaletteController::handle_event(const ftxui::Event& event)
{
    if (event == ftxui::Event::Escape)
    {
        close();
        return true;
    }

    const bool close_open_file_mode = _model.mode == CommandPaletteMode::CloseOpenFile;

    if (_command_history != nullptr && event == ftxui::Event::CtrlR && !close_open_file_mode)
    {
        _model.mode           = _model.mode == CommandPaletteMode::Commands ? CommandPaletteMode::History : CommandPaletteMode::Commands;
        _model.selected_index = 0;
        _model.status_message.clear();
        _model.status_is_error = false;
        refresh_matches();
        return true;
    }

    if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k'))
    {
        move_selection(-1);
        return true;
    }

    if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j'))
    {
        move_selection(1);
        return true;
    }

    if (close_open_file_mode && event != ftxui::Event::Return)
    {
        return true;
    }

    if (event == ftxui::Event::ArrowLeft)
    {
        _model.cursor_position = previous_codepoint_start(_model.query, _model.cursor_position);
        return true;
    }

    if (event == ftxui::Event::ArrowRight)
    {
        _model.cursor_position = next_codepoint_end(_model.query, _model.cursor_position);
        return true;
    }

    if (event == ftxui::Event::Home)
    {
        _model.cursor_position = 0;
        return true;
    }

    if (event == ftxui::Event::End)
    {
        _model.cursor_position = _model.query.size();
        return true;
    }

    if (event == ftxui::Event::Backspace)
    {
        const std::size_t erase_start = previous_codepoint_start(_model.query, _model.cursor_position);
        if (erase_start != _model.cursor_position)
        {
            _model.query.erase(erase_start, _model.cursor_position - erase_start);
            _model.cursor_position = erase_start;
            _model.status_message.clear();
            _model.status_is_error = false;
            refresh_matches();
        }

        return true;
    }

    if (event == ftxui::Event::Delete)
    {
        const std::size_t erase_end = next_codepoint_end(_model.query, _model.cursor_position);
        if (erase_end != _model.cursor_position)
        {
            _model.query.erase(_model.cursor_position, erase_end - _model.cursor_position);
            _model.status_message.clear();
            _model.status_is_error = false;
            refresh_matches();
        }

        return true;
    }

    if (event == ftxui::Event::Return)
    {
        CommandResult result;
        if (_model.mode == CommandPaletteMode::History)
        {
            result = execute_command_from_history_mode();
        }
        else if (_model.mode == CommandPaletteMode::CloseOpenFile)
        {
            result = execute_close_open_file_selection();
        }
        else
        {
            result = execute_command_from_command_mode();
        }

        _model.status_message  = result.message;
        _model.status_is_error = !result.success;
        if (result.success && result.close_palette_on_success)
        {
            close();
        }

        return true;
    }

    if (event == ftxui::Event::Tab)
    {
        if (_model.mode == CommandPaletteMode::History)
        {
            copy_selected_history_entry_to_query();
        }
        else if (_model.mode == CommandPaletteMode::Commands)
        {
            autocomplete_selected_command();
        }

        return true;
    }

    if (event.is_character())
    {
        const std::string typed_text = event.character();
        _model.query.insert(_model.cursor_position, typed_text);
        _model.cursor_position += typed_text.size();
        _model.status_message.clear();
        _model.status_is_error = false;
        refresh_matches();
        return true;
    }

    return true;
}

void CommandPaletteController::autocomplete_selected_command()
{
    if (_model.mode != CommandPaletteMode::Commands)
    {
        return;
    }

    if (_model.selected_index < 0 || static_cast<std::size_t>(_model.selected_index) >= _model.matching_commands.size())
    {
        return;
    }

    const auto& selected_command            = _model.matching_commands[static_cast<std::size_t>(_model.selected_index)];
    const auto [command_start, command_end] = command_name_range(_model.query);
    _model.query.replace(command_start, command_end - command_start, selected_command.name);
    _model.cursor_position = command_start + selected_command.name.size();
    _model.status_message.clear();
    _model.status_is_error = false;
    refresh_matches();
}

void CommandPaletteController::refresh_matches()
{
    if (_model.mode == CommandPaletteMode::History)
    {
        _model.matching_commands.clear();
        _model.open_files.clear();
        if (_command_history != nullptr)
        {
            _model.matching_history_entries = _command_history->matching_entries(_model.query);
        }
        else
        {
            _model.matching_history_entries.clear();
        }
    }
    else if (_model.mode == CommandPaletteMode::Commands)
    {
        _model.matching_history_entries.clear();
        _model.open_files.clear();
        _model.matching_commands = _command_manager.matching_commands(_model.query);
    }
    else
    {
        _model.matching_history_entries.clear();
        _model.matching_commands.clear();
    }

    if (active_match_count() == 0)
    {
        _model.selected_index = 0;
        return;
    }

    _model.selected_index = std::clamp(_model.selected_index, 0, static_cast<int>(active_match_count()) - 1);
}

std::size_t CommandPaletteController::active_match_count() const
{
    if (_model.mode == CommandPaletteMode::History)
    {
        return _model.matching_history_entries.size();
    }

    if (_model.mode == CommandPaletteMode::CloseOpenFile)
    {
        return _model.open_files.size();
    }

    return _model.matching_commands.size();
}

void CommandPaletteController::move_selection(int delta)
{
    const std::size_t match_count = active_match_count();
    if (match_count == 0)
    {
        return;
    }

    const int last_index  = static_cast<int>(match_count) - 1;
    _model.selected_index = std::clamp(_model.selected_index + delta, 0, last_index);
}

bool CommandPaletteController::copy_selected_history_entry_to_query()
{
    if (_model.mode != CommandPaletteMode::History)
    {
        return false;
    }

    if (_model.selected_index < 0 || static_cast<std::size_t>(_model.selected_index) >= _model.matching_history_entries.size())
    {
        return false;
    }

    _model.query           = _model.matching_history_entries[static_cast<std::size_t>(_model.selected_index)];
    _model.cursor_position = _model.query.size();
    _model.mode            = CommandPaletteMode::Commands;
    _model.selected_index  = 0;
    _model.status_message.clear();
    _model.status_is_error = false;
    refresh_matches();
    return true;
}

CommandResult CommandPaletteController::execute_command_from_command_mode()
{
    std::string command_line;
    if (!_model.matching_commands.empty())
    {
        const auto& selected_command = _model.matching_commands[static_cast<std::size_t>(_model.selected_index)];
        const std::string arguments  = command_arguments_from_query(_model.query);
        command_line                 = arguments.empty() ? selected_command.name : selected_command.name + " " + arguments;
    }
    else
    {
        command_line = _model.query;
    }

    CommandResult result = _command_manager.execute(command_line);
    if (result.success && !record_successful_command(command_line))
    {
        result.message += " (failed to save history)";
    }

    return result;
}

CommandResult CommandPaletteController::execute_command_from_history_mode()
{
    if (_command_history == nullptr)
    {
        return {false, "Command history is not available."};
    }

    if (_model.matching_history_entries.empty())
    {
        CommandResult result = _command_manager.execute(_model.query);
        if (result.success && !record_successful_command(_model.query))
        {
            result.message += " (failed to save history)";
        }

        return result;
    }

    const std::string& command_line = _model.matching_history_entries[static_cast<std::size_t>(_model.selected_index)];
    CommandResult result            = _command_manager.execute(command_line);
    if (result.success && !record_successful_command(command_line))
    {
        result.message += " (failed to save history)";
    }

    return result;
}

CommandResult CommandPaletteController::execute_close_open_file_selection()
{
    if (_close_open_file_selection_handler == nullptr)
    {
        return {false, "No close-file handler is configured."};
    }

    if (_model.selected_index < 0 || static_cast<std::size_t>(_model.selected_index) >= _model.open_files.size())
    {
        return {false, "No open file is selected."};
    }

    return _close_open_file_selection_handler(static_cast<std::size_t>(_model.selected_index));
}

bool CommandPaletteController::record_successful_command(std::string_view command_line)
{
    if (_command_history == nullptr)
    {
        return true;
    }

    std::string error_message;
    return _command_history->record_command(command_line, error_message);
}

} // namespace slayerlog
