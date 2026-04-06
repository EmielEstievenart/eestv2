#pragma once

#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "command_palette_controller.hpp"
#include "log_controller.hpp"
#include "log_model.hpp"
#include "log_view.hpp"

namespace slayerlog
{

class MasterController
{
public:
    MasterController(LogModel& model, LogController& log_controller, LogView& log_view, ftxui::ScreenInteractive& screen,
                     CommandPaletteController& command_palette_controller);

    bool handle_event(const ftxui::Event& event);
    bool exit_requested() const;

private:
    LogModel& _model;
    LogController& _log_controller;
    LogView& _log_view;
    ftxui::ScreenInteractive& _screen;
    CommandPaletteController& _command_palette_controller;
    bool _exit_requested = false;
};

} // namespace slayerlog
