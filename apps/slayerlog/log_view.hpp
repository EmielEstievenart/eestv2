#pragma once

#include <optional>
#include <cstddef>
#include <string>
#include <vector>

#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include "command_palette_model.hpp"
#include "log_view_model.hpp"

namespace slayerlog
{

class LogView
{
public:
    ftxui::Element render(LogViewModel& model, const std::string& header_text, int screen_height,
                          const CommandPaletteModel& command_palette);
    std::optional<TextPosition> mouse_to_text_position(const LogViewModel& model, const ftxui::Mouse& mouse) const;

private:
    ftxui::Box _viewport_box;
};

} // namespace slayerlog
