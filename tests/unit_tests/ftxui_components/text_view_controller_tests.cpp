#include <gtest/gtest.h>

#include <ftxui/component/event.hpp>
#include <ftxui_components/text_view_controller.hpp>
#include <ftxui_components/text_view_model.hpp>

TEST(TextViewControllerTest, CtrlArrowKeysScrollHorizontallyByViewportWidthMinusOne)
{
    TextViewModel model;
    model.append_line("0123456789abcdef");

    TextViewController controller(model);
    controller.update_viewport_col_count(5);

    EXPECT_TRUE(controller.parse_event(ftxui::Event::ArrowRightCtrl, {}));
    EXPECT_EQ(controller.render_data(1).first_visible_col, 2);

    EXPECT_TRUE(controller.parse_event(ftxui::Event::ArrowRightCtrl, {}));
    EXPECT_EQ(controller.render_data(1).first_visible_col, 4);

    EXPECT_TRUE(controller.parse_event(ftxui::Event::ArrowLeftCtrl, {}));
    EXPECT_EQ(controller.render_data(1).first_visible_col, 2);

    EXPECT_TRUE(controller.parse_event(ftxui::Event::ArrowLeftCtrl, {}));
    EXPECT_EQ(controller.render_data(1).first_visible_col, 0);
}
