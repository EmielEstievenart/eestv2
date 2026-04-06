#include <gtest/gtest.h>

#include <ftxui/component/screen_interactive.hpp>

#include "command_manager.hpp"
#include "command_palette_controller.hpp"
#include "command_palette_model.hpp"
#include "log_controller.hpp"
#include "log_model.hpp"
#include "log_view.hpp"
#include "master_controller.hpp"

namespace slayerlog
{

TEST(MasterControllerTest, CtrlPOpensCommandPalette)
{
    LogModel model;
    LogController controller;
    CommandPaletteModel command_palette_model;
    CommandManager command_manager;
    CommandPaletteController command_palette_controller(command_palette_model, command_manager);
    LogView view;
    auto screen = ftxui::ScreenInteractive::FixedSize(80, 24);
    MasterController master_controller(model, controller, view, screen, command_palette_controller);

    EXPECT_TRUE(master_controller.handle_event(ftxui::Event::CtrlP));
    EXPECT_TRUE(command_palette_controller.is_open());
}

TEST(MasterControllerTest, WhenPaletteOpenInputRoutesToPalette)
{
    LogModel model;
    LogController controller;
    model.append_lines({
        ObservedLogLine {"alpha.log", "line one"},
        ObservedLogLine {"alpha.log", "line two"},
        ObservedLogLine {"alpha.log", "line three"},
    });
    controller.scroll_to_top(model, 1);
    EXPECT_EQ(controller.first_visible_line_index(model, 1).value, 0);

    CommandPaletteModel command_palette_model;
    CommandManager command_manager;
    command_manager.register_command({"alpha", "First", "alpha"}, [](std::string_view) { return CommandResult {true, "ok"}; });
    CommandPaletteController command_palette_controller(command_palette_model, command_manager);
    command_palette_controller.open();

    LogView view;
    auto screen = ftxui::ScreenInteractive::FixedSize(80, 24);
    MasterController master_controller(model, controller, view, screen, command_palette_controller);

    EXPECT_TRUE(master_controller.handle_event(ftxui::Event::ArrowDown));
    EXPECT_EQ(controller.first_visible_line_index(model, 1).value, 0);
}

TEST(MasterControllerTest, EscapeClosesPaletteBeforeExiting)
{
    LogModel model;
    LogController controller;
    CommandPaletteModel command_palette_model;
    CommandManager command_manager;
    CommandPaletteController command_palette_controller(command_palette_model, command_manager);
    command_palette_controller.open();

    LogView view;
    auto screen = ftxui::ScreenInteractive::FixedSize(80, 24);
    MasterController master_controller(model, controller, view, screen, command_palette_controller);

    EXPECT_TRUE(master_controller.handle_event(ftxui::Event::Escape));
    EXPECT_FALSE(command_palette_controller.is_open());
    EXPECT_FALSE(master_controller.exit_requested());
}

TEST(MasterControllerTest, EscapeExitsWhenPaletteClosedAndFindInactive)
{
    LogModel model;
    LogController controller;
    CommandPaletteModel command_palette_model;
    CommandManager command_manager;
    CommandPaletteController command_palette_controller(command_palette_model, command_manager);

    LogView view;
    auto screen = ftxui::ScreenInteractive::FixedSize(80, 24);
    MasterController master_controller(model, controller, view, screen, command_palette_controller);

    EXPECT_TRUE(master_controller.handle_event(ftxui::Event::Escape));
    EXPECT_TRUE(master_controller.exit_requested());
}

} // namespace slayerlog
