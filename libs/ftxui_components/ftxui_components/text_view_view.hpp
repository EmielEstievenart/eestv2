#pragma once

#include <optional>

#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include <ftxui_components/text_view_controller.hpp>

class TextViewView
{
public:
    TextViewView() = default;
    explicit TextViewView(TextViewController& controller);

    [[nodiscard]] ftxui::Element component();
    [[nodiscard]] ftxui::Element render(const TextViewRenderData& data);
    [[nodiscard]] int viewport_line_count() const;
    [[nodiscard]] int viewport_col_count() const;
    [[nodiscard]] std::optional<TextViewPosition> mouse_to_text_position(const TextViewRenderData& data, const ftxui::Mouse& mouse) const;

private:
    [[nodiscard]] TextViewRenderData normalize_render_data(TextViewRenderData data) const;

    TextViewController* _controller = nullptr;

    ftxui::Box _box;
    ftxui::Box _content_box;

    ftxui::Element render_scrollbar(const TextViewRenderData& data);
    ftxui::Element render_hscrollbar(const TextViewRenderData& data);
};
