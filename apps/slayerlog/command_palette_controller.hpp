#pragma once

#include <functional>
#include <string>
#include <vector>

#include <ftxui/component/event.hpp>

#include "command_history.hpp"
#include "command_manager.hpp"
#include "command_palette_model.hpp"

namespace slayerlog
{

class CommandPaletteController
{
public:
    CommandPaletteController(CommandPaletteModel& model, CommandManager& command_manager);
    CommandPaletteController(CommandPaletteModel& model, CommandManager& command_manager, CommandHistory& command_history);

    bool is_open() const;
    const CommandPaletteModel& model() const;

    void open();
    void open_close_open_file_picker(std::vector<std::string> open_files,
                                     std::function<CommandResult(std::size_t selected_index)> on_confirm);
    void close();
    bool handle_event(const ftxui::Event& event);

private:
    void autocomplete_selected_command();
    void refresh_matches();
    std::size_t active_match_count() const;
    void move_selection(int delta);
    bool copy_selected_history_entry_to_query();
    CommandResult execute_command_from_command_mode();
    CommandResult execute_command_from_history_mode();
    CommandResult execute_close_open_file_selection();
    bool record_successful_command(std::string_view command_line);

    CommandPaletteModel& _model;
    CommandManager& _command_manager;
    CommandHistory* _command_history = nullptr;
    std::function<CommandResult(std::size_t selected_index)> _close_open_file_selection_handler;
};

} // namespace slayerlog
