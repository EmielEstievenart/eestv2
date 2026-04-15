#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui_components/text_view_controller.hpp>
#include <ftxui_components/text_view_model.hpp>

TEST(TextViewControllerTest, CtrlArrowKeysScrollHorizontallyByViewportWidthMinusOne)
{
    std::vector<std::string> lines = {"0123456789abcdef"};
    TextViewModel model;
    model.set_lines(lines);

    TextViewController controller(model);
    controller.update_viewport_line_count(1);
    controller.update_viewport_col_count(5);

    EXPECT_TRUE(controller.parse_event(ftxui::Event::ArrowRightCtrl, {}).handled);
    EXPECT_EQ(controller.render_data().first_visible_col, 2);

    EXPECT_TRUE(controller.parse_event(ftxui::Event::ArrowRightCtrl, {}).handled);
    EXPECT_EQ(controller.render_data().first_visible_col, 4);

    EXPECT_TRUE(controller.parse_event(ftxui::Event::ArrowLeftCtrl, {}).handled);
    EXPECT_EQ(controller.render_data().first_visible_col, 2);

    EXPECT_TRUE(controller.parse_event(ftxui::Event::ArrowLeftCtrl, {}).handled);
    EXPECT_EQ(controller.render_data().first_visible_col, 0);
}

TEST(TextViewControllerTest, SwapLinesUpdatesModelAndClearsSelection)
{
    std::vector<std::string> lines_a = {"line one", "line two"};
    std::vector<std::string> lines_b = {"alpha", "beta", "gamma"};

    TextViewModel model;
    TextViewController controller(model);
    controller.update_viewport_line_count(10);
    controller.update_viewport_col_count(80);

    controller.swap_lines(lines_a);
    EXPECT_EQ(controller.render_data().total_lines, 2);

    // Start a selection
    controller.begin_selection({0, 0});
    controller.update_selection({0, 4});
    EXPECT_TRUE(controller.selection_bounds().has_value());

    // Swap to new lines — selection should be cleared
    controller.swap_lines(lines_b);
    EXPECT_EQ(controller.render_data().total_lines, 3);
    EXPECT_FALSE(controller.selection_bounds().has_value());
}

TEST(TextViewControllerTest, NotifyLinesAppendedAutoScrollsWhenFollowingBottom)
{
    std::vector<std::string> lines = {"line 1", "line 2"};

    TextViewModel model;
    model.set_lines(lines);

    TextViewController controller(model);
    controller.update_viewport_line_count(2);

    // follow_bottom is true by default
    EXPECT_TRUE(controller.follow_bottom());
    EXPECT_EQ(controller.first_visible_line(), 0);

    // Append lines and notify
    lines.push_back("line 3");
    lines.push_back("line 4");
    lines.push_back("line 5");
    controller.notify_lines_appended();

    // Should auto-scroll to bottom: first_visible = 5 - 2 = 3
    EXPECT_EQ(controller.first_visible_line(), 3);
    EXPECT_TRUE(controller.follow_bottom());
}

TEST(TextViewControllerTest, CenterOnLineCentersViewport)
{
    std::vector<std::string> lines;
    for (int i = 0; i < 100; ++i)
    {
        lines.push_back("line " + std::to_string(i));
    }

    TextViewModel model;
    model.set_lines(lines);

    TextViewController controller(model);
    controller.update_viewport_line_count(10);

    controller.center_on_line(50);
    EXPECT_EQ(controller.first_visible_line(), 45); // 50 - 10/2 = 45
    EXPECT_FALSE(controller.follow_bottom());
}

TEST(TextViewControllerTest, SelectionTextReturnsCorrectContent)
{
    std::vector<std::string> lines = {"Hello World", "Second Line", "Third Line"};

    TextViewModel model;
    model.set_lines(lines);

    TextViewController controller(model);
    controller.update_viewport_line_count(10);
    controller.update_viewport_col_count(80);

    controller.begin_selection({0, 6});
    controller.update_selection({1, 6});
    controller.end_selection(TextViewPosition {1, 6});

    EXPECT_EQ(controller.selection_text(), "World\nSecond");
}

TEST(TextViewControllerTest, RenderDataIncludesSelectionDecorations)
{
    std::vector<std::string> lines = {"Hello World"};

    TextViewModel model;
    model.set_lines(lines);

    TextViewController controller(model);
    controller.update_viewport_line_count(10);
    controller.update_viewport_col_count(80);

    controller.begin_selection({0, 0});
    controller.update_selection({0, 5});
    controller.end_selection(TextViewPosition {0, 5});

    auto data = controller.render_data();
    ASSERT_EQ(data.range_decorations.size(), 1u);
    EXPECT_EQ(data.range_decorations[0].line_index, 0);
    EXPECT_EQ(data.range_decorations[0].col_start, 0);
    EXPECT_EQ(data.range_decorations[0].col_end, 5);
    EXPECT_TRUE(data.range_decorations[0].style.inverted);
}

TEST(TextViewControllerTest, ParseEventReturnsRequestExitOnQuit)
{
    std::vector<std::string> lines = {"test"};

    TextViewModel model;
    model.set_lines(lines);

    TextViewController controller(model);

    auto result = controller.parse_event(ftxui::Event::Character('q'), {});
    EXPECT_TRUE(result.handled);
    EXPECT_TRUE(result.request_exit);
}

TEST(TextViewControllerTest, ParseEventReturnsRequestExitOnEscape)
{
    std::vector<std::string> lines = {"test"};

    TextViewModel model;
    model.set_lines(lines);

    TextViewController controller(model);

    auto result = controller.parse_event(ftxui::Event::Escape, {});
    EXPECT_TRUE(result.handled);
    EXPECT_TRUE(result.request_exit);
}
