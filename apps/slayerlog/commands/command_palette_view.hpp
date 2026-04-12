#pragma once

#include <ftxui/dom/elements.hpp>

#include "command_palette_model.hpp"

namespace slayerlog
{

class CommandPaletteView
{
public:
    ftxui::Element render(const CommandPaletteModel& command_palette) const;
};

} // namespace slayerlog
