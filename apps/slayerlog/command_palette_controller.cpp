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
    _model.query.clear();
    _model.cursor_position = 0;
    _model.selected_index  = 0;
    _model.status_message.clear();
    _model.status_is_error = false;
    refresh_matches();
}

void CommandPaletteController::close()
{
    _model.open = false;
    _model.query.clear();
    _model.cursor_position = 0;
    _model.selected_index  = 0;
    refresh_matches();
}

bool CommandPaletteController::handle_event(const ftxui::Event& event)
{
    if (event == ftxui::Event::Escape)
    {
        close();
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
        const CommandResult result = execute_selected_command();
        _model.status_message      = result.message;
        _model.status_is_error     = !result.success;
        if (result.success)
        {
            close();
        }

        return true;
    }

    if (event == ftxui::Event::Tab)
    {
        autocomplete_selected_command();
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
    _model.matching_commands = _command_manager.matching_commands(_model.query);
    if (_model.matching_commands.empty())
    {
        _model.selected_index = 0;
        return;
    }

    _model.selected_index = std::clamp(_model.selected_index, 0, static_cast<int>(_model.matching_commands.size()) - 1);
}

void CommandPaletteController::move_selection(int delta)
{
    if (_model.matching_commands.empty())
    {
        return;
    }

    const int last_index  = static_cast<int>(_model.matching_commands.size()) - 1;
    _model.selected_index = std::clamp(_model.selected_index + delta, 0, last_index);
}

CommandResult CommandPaletteController::execute_selected_command()
{
    if (!_model.matching_commands.empty())
    {
        const auto& selected_command   = _model.matching_commands[static_cast<std::size_t>(_model.selected_index)];
        const std::string arguments    = command_arguments_from_query(_model.query);
        const std::string command_line = arguments.empty() ? selected_command.name : selected_command.name + " " + arguments;
        return _command_manager.execute(command_line);
    }

    return _command_manager.execute(_model.query);
}

} // namespace slayerlog
