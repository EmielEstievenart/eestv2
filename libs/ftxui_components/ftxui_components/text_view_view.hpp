#pragma once

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include <ftxui_components/text_view_controller.hpp>

class TextViewView
{
public:
    explicit TextViewView(TextViewController& controller);

    [[nodiscard]] ftxui::Element component();

private:
    TextViewController& _controller;

    ftxui::Box _box;
    ftxui::Box _content_box;

    ftxui::Element render_scrollbar(const TextViewRenderData& data);
    ftxui::Element render_hscrollbar(const TextViewRenderData& data);
};
