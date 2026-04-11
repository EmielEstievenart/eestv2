#include "master_view.hpp"

namespace slayerlog
{

MasterView::MasterView(LogView& log_view, CommandPaletteView& command_palette_view) : _log_view(log_view), _command_palette_view(command_palette_view)
{
}

ftxui::Element MasterView::render(const LogModel& model, const LogController& controller, const std::string& header_text, int screen_height, const CommandPaletteModel& command_palette) const
{
    auto base_view = _log_view.render(model, controller, header_text, screen_height, command_palette.hidden_column_preview);
    if (!command_palette.open)
    {
        return base_view;
    }

    return ftxui::dbox({
        std::move(base_view),
        _command_palette_view.render(command_palette),
    });
}

} // namespace slayerlog
