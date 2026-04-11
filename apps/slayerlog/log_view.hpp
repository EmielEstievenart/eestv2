#pragma once

#include <optional>
#include <cstddef>
#include <string>
#include <vector>

#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>

#include <ftxui_components/text_view_view.hpp>

#include "log_controller.hpp"
#include "log_model.hpp"

namespace slayerlog
{

class LogView
{
public:
    int visible_line_count(int screen_height) const;
    int visible_col_count() const;
    ftxui::Element render(const LogModel& model, const LogController& controller, const std::string& header_text, int screen_height);
    std::optional<TextPosition> mouse_to_text_position(const LogModel& model, const LogController& controller, const ftxui::Mouse& mouse) const;

private:
    TextViewView _text_view;
};

} // namespace slayerlog
