#pragma once

#include <ftxui/component/event.hpp>

#include "command_manager.hpp"
#include "command_palette_model.hpp"

namespace slayerlog
{

class CommandPaletteController
{
public:
    CommandPaletteController(CommandPaletteModel& model, CommandManager& command_manager);

    bool is_open() const;
    const CommandPaletteModel& model() const;

    void open();
    void close();
    bool handle_event(const ftxui::Event& event);

private:
    void autocomplete_selected_command();
    void refresh_matches();
    void move_selection(int delta);
    CommandResult execute_selected_command();

    CommandPaletteModel& _model;
    CommandManager& _command_manager;
};

} // namespace slayerlog
