#pragma once

#include <optional>
#include <string>

#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include "log_view_model.hpp"

namespace slayerlog
{

class LogView
{
public:
    ftxui::Element render(LogViewModel& model, const std::string& header_text);
    std::optional<TextPosition> mouse_to_text_position(const LogViewModel& model, const ftxui::Mouse& mouse) const;

private:
    ftxui::Box _viewport_box;
};

} // namespace slayerlog
