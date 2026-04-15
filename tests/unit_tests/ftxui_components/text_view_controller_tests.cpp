#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui_components/text_view_controller.hpp>

namespace
{

TextViewController make_controller(std::vector<std::string>& lines)
{
    int max_line_width = 0;
    for (const auto& line : lines)
    {
        max_line_width = std::max(max_line_width, static_cast<int>(line.size()));
    }

    TextViewController controller;
    controller.set_content(static_cast<int>(lines.size()), max_line_width, [&lines](int index) -> const std::string& { return lines[static_cast<std::size_t>(index)]; });
    return controller;
}

} // namespace

TEST(TextViewControllerTest, CtrlArrowKeysScrollHorizontallyByViewportWidthMinusOne)
{
    std::vector<std::string> lines = {"0123456789abcdef"};
    TextViewController controller  = make_controller(lines);
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

TEST(TextViewControllerTest, SetContentUpdatesContentAndClearsSelection)
{
    std::vector<std::string> lines_a = {"line one", "line two"};
    std::vector<std::string> lines_b = {"alpha", "beta", "gamma"};

    TextViewController controller = make_controller(lines_a);
    controller.update_viewport_line_count(10);
    controller.update_viewport_col_count(80);

    EXPECT_EQ(controller.render_data().total_lines, 2);

    // Start a selection
    controller.begin_selection({0, 0});
    controller.update_selection({0, 4});
    EXPECT_TRUE(controller.selection_bounds().has_value());

    int max_line_width = 0;
    for (const auto& line : lines_b)
    {
        max_line_width = std::max(max_line_width, static_cast<int>(line.size()));
    }

    // Replace content -- selection should be cleared
    controller.set_content(static_cast<int>(lines_b.size()), max_line_width, [&lines_b](int index) -> const std::string& { return lines_b[static_cast<std::size_t>(index)]; });
    EXPECT_EQ(controller.render_data().total_lines, 3);
    EXPECT_FALSE(controller.selection_bounds().has_value());
}

TEST(TextViewControllerTest, UpdateContentSizeAutoScrollsWhenFollowingBottom)
{
    std::vector<std::string> lines = {"line 1", "line 2"};
    TextViewController controller  = make_controller(lines);
    controller.update_viewport_line_count(2);

    // follow_bottom is true by default
    EXPECT_TRUE(controller.follow_bottom());
    EXPECT_EQ(controller.first_visible_line(), 0);

    // Append lines and notify
    lines.push_back("line 3");
    lines.push_back("line 4");
    lines.push_back("line 5");
    controller.update_content_size(5, static_cast<int>(std::string("line 5").size()));

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

    TextViewController controller = make_controller(lines);
    controller.update_viewport_line_count(10);

    controller.center_on_line(50);
    EXPECT_EQ(controller.first_visible_line(), 45); // 50 - 10/2 = 45
    EXPECT_FALSE(controller.follow_bottom());
}

TEST(TextViewControllerTest, SelectionTextReturnsCorrectContent)
{
    std::vector<std::string> lines = {"Hello World", "Second Line", "Third Line"};
    TextViewController controller  = make_controller(lines);
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
    TextViewController controller  = make_controller(lines);
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
    TextViewController controller  = make_controller(lines);

    auto result = controller.parse_event(ftxui::Event::Character('q'), {});
    EXPECT_TRUE(result.handled);
    EXPECT_TRUE(result.request_exit);
}

TEST(TextViewControllerTest, ParseEventReturnsRequestExitOnEscape)
{
    std::vector<std::string> lines = {"test"};
    TextViewController controller  = make_controller(lines);

    auto result = controller.parse_event(ftxui::Event::Escape, {});
    EXPECT_TRUE(result.handled);
    EXPECT_TRUE(result.request_exit);
}
