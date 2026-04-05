#pragma once

#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "command_palette_controller.hpp"
#include "log_controller.hpp"
#include "log_view.hpp"
#include "log_view_model.hpp"

namespace slayerlog
{

class InputController
{
public:
    InputController(LogViewModel& model, LogController& controller, LogView& view, ftxui::ScreenInteractive& screen,
                    CommandPaletteController& command_palette_controller);

    bool handle_event(ftxui::Event event);
    const CommandPaletteModel& command_palette() const;

private:
    bool copy_selection_to_clipboard() const;

    LogViewModel& _model;
    LogController& _controller;
    LogView& _view;
    ftxui::ScreenInteractive& _screen;
    CommandPaletteController& _command_palette_controller;
};

} // namespace slayerlog
