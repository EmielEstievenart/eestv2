#include "master_controller.hpp"

namespace slayerlog
{

MasterController::MasterController(ProcessedSources& processed_sources, LogController& log_controller, LogView& log_view, ftxui::ScreenInteractive& screen, CommandPaletteController& command_palette_controller)
    : _processed_sources(processed_sources), _log_controller(log_controller), _log_view(log_view), _screen(screen), _command_palette_controller(command_palette_controller)
{
}

bool MasterController::handle_event(const ftxui::Event& event)
{
    _exit_requested = false;

    if (event == ftxui::Event::CtrlP)
    {
        _command_palette_controller.open();
        return true;
    }

    if (_command_palette_controller.is_open())
    {
        return _command_palette_controller.handle_event(event);
    }

    if (event == ftxui::Event::CtrlR)
    {
        _command_palette_controller.open_history();
        return true;
    }

    const auto result = _log_controller.handle_event(_processed_sources, event, [this](const ftxui::Mouse& mouse) { return _log_view.mouse_to_text_position(_log_controller, mouse); });

    if (result.request_exit)
    {
        _exit_requested = true;
        _screen.Exit();
    }

    return result.handled;
}

bool MasterController::exit_requested() const
{
    return _exit_requested;
}

} // namespace slayerlog
