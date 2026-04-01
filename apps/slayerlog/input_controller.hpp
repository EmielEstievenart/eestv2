#pragma once

#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "log_view.hpp"
#include "log_view_model.hpp"

namespace slayerlog
{

class InputController
{
public:
    InputController(LogViewModel& model, LogView& view, ftxui::ScreenInteractive& screen);

    bool handle_event(ftxui::Event event);

private:
    bool copy_selection_to_clipboard() const;

    LogViewModel& _model;
    LogView& _view;
    ftxui::ScreenInteractive& _screen;
};

} // namespace slayerlog
