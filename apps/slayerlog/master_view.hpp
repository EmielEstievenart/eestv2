#pragma once

#include <string>

#include <ftxui/dom/elements.hpp>

#include "command_palette_model.hpp"
#include "command_palette_view.hpp"
#include "log_controller.hpp"
#include "log_model.hpp"
#include "log_view.hpp"

namespace slayerlog
{

class MasterView
{
public:
    MasterView(LogView& log_view, CommandPaletteView& command_palette_view);

    ftxui::Element render(const LogModel& model, const LogController& controller, const std::string& header_text, int screen_height,
                          const CommandPaletteModel& command_palette) const;

private:
    LogView& _log_view;
    CommandPaletteView& _command_palette_view;
};

} // namespace slayerlog
