#include <gtest/gtest.h>

#include <ftxui/component/screen_interactive.hpp>

#include "command_manager.hpp"
#include "command_palette_controller.hpp"
#include "command_palette_model.hpp"
#include "input_controller.hpp"
#include "log_controller.hpp"
#include "log_view.hpp"
#include "log_model.hpp"

namespace slayerlog
{

TEST(InputControllerTest, EscapeClearsFindBeforeQuitting)
{
    LogModel model;
    LogController controller;
    model.append_lines({
        ObservedLogLine {"alpha.log", "error one"},
        ObservedLogLine {"alpha.log", "error two"},
    });
    ASSERT_TRUE(controller.set_find_query(model, "error", 1));
    ASSERT_TRUE(model.find_active());

    CommandPaletteModel command_palette_model;
    CommandManager command_manager;
    CommandPaletteController command_palette_controller(command_palette_model, command_manager);
    LogView view;
    auto screen = ftxui::ScreenInteractive::FixedSize(80, 24);
    InputController input_controller(model, controller, view, screen, command_palette_controller);

    EXPECT_TRUE(input_controller.handle_event(ftxui::Event::Escape));
    EXPECT_FALSE(model.find_active());
    EXPECT_EQ(model.total_find_match_count(), 0);
}

TEST(InputControllerTest, LeftAndRightArrowNavigateFindResults)
{
    LogModel model;
    LogController controller;
    model.append_lines({
        ObservedLogLine {"alpha.log", "error one"},
        ObservedLogLine {"alpha.log", "error two"},
        ObservedLogLine {"alpha.log", "error three"},
    });
    ASSERT_TRUE(controller.set_find_query(model, "error", 1));
    ASSERT_TRUE(controller.go_to_next_find_match(model, 1));
    ASSERT_TRUE(controller.active_find_visible_index(model).has_value());
    EXPECT_EQ(controller.active_find_visible_index(model)->value, 1);

    CommandPaletteModel command_palette_model;
    CommandManager command_manager;
    CommandPaletteController command_palette_controller(command_palette_model, command_manager);
    LogView view;
    auto screen = ftxui::ScreenInteractive::FixedSize(80, 24);
    InputController input_controller(model, controller, view, screen, command_palette_controller);

    EXPECT_TRUE(input_controller.handle_event(ftxui::Event::ArrowLeft));
    ASSERT_TRUE(controller.active_find_visible_index(model).has_value());
    EXPECT_EQ(controller.active_find_visible_index(model)->value, 0);

    EXPECT_TRUE(input_controller.handle_event(ftxui::Event::ArrowRight));
    ASSERT_TRUE(controller.active_find_visible_index(model).has_value());
    EXPECT_EQ(controller.active_find_visible_index(model)->value, 1);
}

} // namespace slayerlog
