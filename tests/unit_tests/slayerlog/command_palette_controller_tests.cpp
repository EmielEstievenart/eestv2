#include <gtest/gtest.h>

#include <string>

#include <ftxui/component/event.hpp>

#include "command_palette_controller.hpp"

namespace slayerlog
{

TEST(CommandPaletteControllerTest, OpenResetsSessionStateAndLoadsAllCommands)
{
    CommandManager manager;
    manager.register_command({"alpha", "First", "alpha"}, [](std::string_view) { return CommandResult {true, "ok"}; });
    manager.register_command({"beta", "Second", "beta"}, [](std::string_view) { return CommandResult {true, "ok"}; });

    CommandPaletteModel model;
    model.open            = false;
    model.query           = "stale";
    model.cursor_position = 3;
    model.selected_index  = 10;
    model.status_message  = "error";
    model.status_is_error = true;

    CommandPaletteController controller(model, manager);
    controller.open();

    EXPECT_TRUE(controller.is_open());
    EXPECT_TRUE(controller.model().query.empty());
    EXPECT_EQ(controller.model().cursor_position, 0U);
    EXPECT_EQ(controller.model().selected_index, 0);
    EXPECT_TRUE(controller.model().status_message.empty());
    EXPECT_FALSE(controller.model().status_is_error);
    ASSERT_EQ(controller.model().matching_commands.size(), 2U);
    EXPECT_EQ(controller.model().matching_commands[0].name, "alpha");
    EXPECT_EQ(controller.model().matching_commands[1].name, "beta");
}

TEST(CommandPaletteControllerTest, CharacterInputUpdatesQueryAndMatchingCommands)
{
    CommandManager manager;
    manager.register_command({"alpha", "First", "alpha"}, [](std::string_view) { return CommandResult {true, "ok"}; });
    manager.register_command({"beta", "Second", "beta"}, [](std::string_view) { return CommandResult {true, "ok"}; });

    CommandPaletteModel model;
    CommandPaletteController controller(model, manager);
    controller.open();

    EXPECT_TRUE(controller.handle_event(ftxui::Event::Character('b')));

    EXPECT_EQ(controller.model().query, "b");
    EXPECT_EQ(controller.model().cursor_position, 1U);
    ASSERT_EQ(controller.model().matching_commands.size(), 1U);
    EXPECT_EQ(controller.model().matching_commands[0].name, "beta");
}

TEST(CommandPaletteControllerTest, ReturnExecutesSelectedCommandWithArgumentsAndClosesOnSuccess)
{
    CommandManager manager;
    std::string executed_arguments;
    manager.register_command({"beta", "Second", "beta <args>"},
                             [&](std::string_view arguments)
                             {
                                 executed_arguments = std::string(arguments);
                                 return CommandResult {true, "done"};
                             });

    CommandPaletteModel model;
    CommandPaletteController controller(model, manager);
    controller.open();

    ASSERT_TRUE(controller.handle_event(ftxui::Event::Character("beta hello world")));
    ASSERT_TRUE(controller.handle_event(ftxui::Event::Return));

    EXPECT_EQ(executed_arguments, "hello world");
    EXPECT_FALSE(controller.is_open());
    EXPECT_EQ(controller.model().status_message, "done");
    EXPECT_FALSE(controller.model().status_is_error);
}

TEST(CommandPaletteControllerTest, ReturnKeepsPaletteOpenWhenCommandFails)
{
    CommandManager manager;
    manager.register_command({"beta", "Second", "beta"}, [](std::string_view) { return CommandResult {false, "failed"}; });

    CommandPaletteModel model;
    CommandPaletteController controller(model, manager);
    controller.open();

    ASSERT_TRUE(controller.handle_event(ftxui::Event::Character("beta")));
    ASSERT_TRUE(controller.handle_event(ftxui::Event::Return));

    EXPECT_TRUE(controller.is_open());
    EXPECT_EQ(controller.model().status_message, "failed");
    EXPECT_TRUE(controller.model().status_is_error);
}

TEST(CommandPaletteControllerTest, TabCompletesSelectedCommand)
{
    CommandManager manager;
    manager.register_command({"filter-in", "Include matching lines", "filter-in <text>"},
                             [](std::string_view) { return CommandResult {true, "ok"}; });
    manager.register_command({"filter-out", "Exclude matching lines", "filter-out <text>"},
                             [](std::string_view) { return CommandResult {true, "ok"}; });

    CommandPaletteModel model;
    CommandPaletteController controller(model, manager);
    controller.open();

    ASSERT_TRUE(controller.handle_event(ftxui::Event::Character("fil")));
    ASSERT_TRUE(controller.handle_event(ftxui::Event::Tab));

    EXPECT_EQ(controller.model().query, "filter-in");
    EXPECT_EQ(controller.model().cursor_position, std::string("filter-in").size());
}

TEST(CommandPaletteControllerTest, TabPreservesExistingArguments)
{
    CommandManager manager;
    manager.register_command({"filter-in", "Include matching lines", "filter-in <text>"},
                             [](std::string_view) { return CommandResult {true, "ok"}; });
    manager.register_command({"filter-out", "Exclude matching lines", "filter-out <text>"},
                             [](std::string_view) { return CommandResult {true, "ok"}; });

    CommandPaletteModel model;
    CommandPaletteController controller(model, manager);
    controller.open();

    ASSERT_TRUE(controller.handle_event(ftxui::Event::Character("fil error text")));
    ASSERT_TRUE(controller.handle_event(ftxui::Event::Tab));

    EXPECT_EQ(controller.model().query, "filter-in error text");
    EXPECT_EQ(controller.model().cursor_position, std::string("filter-in").size());
}

TEST(CommandPaletteControllerTest, TabLeavesQueryUnchangedWhenNoCommandsMatch)
{
    CommandManager manager;
    manager.register_command({"alpha", "First", "alpha"}, [](std::string_view) { return CommandResult {true, "ok"}; });

    CommandPaletteModel model;
    CommandPaletteController controller(model, manager);
    controller.open();

    ASSERT_TRUE(controller.handle_event(ftxui::Event::Character("missing")));
    ASSERT_TRUE(controller.handle_event(ftxui::Event::Tab));

    EXPECT_EQ(controller.model().query, "missing");
    EXPECT_EQ(controller.model().cursor_position, std::string("missing").size());
}

TEST(CommandPaletteControllerTest, TabClearsStatusMessage)
{
    CommandManager manager;
    manager.register_command({"filter-in", "Include matching lines", "filter-in <text>"},
                             [](std::string_view) { return CommandResult {true, "ok"}; });

    CommandPaletteModel model;
    CommandPaletteController controller(model, manager);
    controller.open();

    ASSERT_TRUE(controller.handle_event(ftxui::Event::Character("fil")));
    model.status_message  = "stale";
    model.status_is_error = true;
    ASSERT_TRUE(controller.handle_event(ftxui::Event::Tab));

    EXPECT_TRUE(controller.model().status_message.empty());
    EXPECT_FALSE(controller.model().status_is_error);
}

TEST(CommandPaletteControllerTest, BackspaceAndDeleteRespectUtf8CodepointBoundaries)
{
    CommandManager manager;
    manager.register_command({"alpha", "First", "alpha"}, [](std::string_view) { return CommandResult {true, "ok"}; });

    CommandPaletteModel model;
    CommandPaletteController controller(model, manager);
    controller.open();

    ASSERT_TRUE(controller.handle_event(ftxui::Event::Character("a测b")));
    ASSERT_TRUE(controller.handle_event(ftxui::Event::ArrowLeft));
    ASSERT_TRUE(controller.handle_event(ftxui::Event::Backspace));

    EXPECT_EQ(controller.model().query, "ab");
    EXPECT_EQ(controller.model().cursor_position, 1U);

    ASSERT_TRUE(controller.handle_event(ftxui::Event::Delete));
    EXPECT_EQ(controller.model().query, "a");
    EXPECT_EQ(controller.model().cursor_position, 1U);
}

TEST(CommandPaletteControllerTest, ReturnExecutesRawQueryWhenNoMatchingCommands)
{
    CommandManager manager;
    manager.register_command({"alpha", "First", "alpha"}, [](std::string_view) { return CommandResult {true, "ok"}; });

    CommandPaletteModel model;
    CommandPaletteController controller(model, manager);
    controller.open();

    ASSERT_TRUE(controller.handle_event(ftxui::Event::Character("missing-command value")));
    ASSERT_TRUE(controller.handle_event(ftxui::Event::Return));

    EXPECT_TRUE(controller.is_open());
    EXPECT_TRUE(controller.model().status_is_error);
    EXPECT_EQ(controller.model().status_message, "Unknown command: missing-command");
}

} // namespace slayerlog
