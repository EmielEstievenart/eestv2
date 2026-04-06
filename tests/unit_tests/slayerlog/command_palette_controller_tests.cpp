#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>

#include <ftxui/component/event.hpp>

#include "command_history.hpp"
#include "command_palette_controller.hpp"
#include "settings_store.hpp"

namespace slayerlog
{

namespace
{

std::filesystem::path make_temp_settings_path()
{
    const auto unique_suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    return std::filesystem::temp_directory_path() / ("slayerlog_command_palette_tests_" + unique_suffix + ".ini");
}

void remove_temp_settings_file(const std::filesystem::path& settings_path)
{
    std::error_code error_code;
    std::filesystem::remove(settings_path, error_code);
    std::filesystem::remove(settings_path.string() + ".tmp", error_code);
}

} // namespace

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

TEST(CommandPaletteControllerTest, CtrlRTogglesHistoryMode)
{
    const auto settings_path = make_temp_settings_path();
    SettingsStore settings_store(settings_path);
    CommandHistory history(settings_store);
    std::string error_message;
    ASSERT_TRUE(history.load(error_message));
    ASSERT_TRUE(history.record_command("find error", error_message));
    ASSERT_TRUE(history.record_command("filter-in auth", error_message));

    CommandManager manager;
    manager.register_command({"find", "Find text", "find <text>"}, [](std::string_view) { return CommandResult {true, "ok"}; });
    manager.register_command({"filter-in", "Filter in", "filter-in <text>"}, [](std::string_view) { return CommandResult {true, "ok"}; });

    CommandPaletteModel model;
    CommandPaletteController controller(model, manager, history);
    controller.open();

    ASSERT_TRUE(controller.handle_event(ftxui::Event::CtrlR));
    EXPECT_EQ(controller.model().mode, CommandPaletteMode::History);
    ASSERT_EQ(controller.model().matching_history_entries.size(), 2U);
    EXPECT_EQ(controller.model().matching_history_entries[0], "filter-in auth");
    EXPECT_EQ(controller.model().matching_history_entries[1], "find error");

    remove_temp_settings_file(settings_path);
}

TEST(CommandPaletteControllerTest, CtrlRWithTextKeepsQueryAndTogglesHistoryMode)
{
    const auto settings_path = make_temp_settings_path();
    SettingsStore settings_store(settings_path);
    CommandHistory history(settings_store);
    std::string error_message;
    ASSERT_TRUE(history.load(error_message));
    ASSERT_TRUE(history.record_command("alpha --first", error_message));
    ASSERT_TRUE(history.record_command("beta second", error_message));

    CommandManager manager;
    manager.register_command({"alpha", "First", "alpha"}, [](std::string_view) { return CommandResult {true, "ok"}; });

    CommandPaletteModel model;
    CommandPaletteController controller(model, manager, history);
    controller.open();

    ASSERT_TRUE(controller.handle_event(ftxui::Event::Character('a')));
    ASSERT_TRUE(controller.handle_event(ftxui::Event::CtrlR));

    EXPECT_EQ(controller.model().mode, CommandPaletteMode::History);
    EXPECT_EQ(controller.model().query, "a");
    ASSERT_EQ(controller.model().matching_history_entries.size(), 2U);
    EXPECT_EQ(controller.model().matching_history_entries[0], "beta second");
    EXPECT_EQ(controller.model().matching_history_entries[1], "alpha --first");

    remove_temp_settings_file(settings_path);
}

TEST(CommandPaletteControllerTest, HistoryModeFiltersAndExecutesSelectedCommand)
{
    const auto settings_path = make_temp_settings_path();
    SettingsStore settings_store(settings_path);
    CommandHistory history(settings_store);
    std::string error_message;
    ASSERT_TRUE(history.load(error_message));
    ASSERT_TRUE(history.record_command("beta first", error_message));
    ASSERT_TRUE(history.record_command("beta second", error_message));

    CommandManager manager;
    std::string executed_arguments;
    manager.register_command({"beta", "Second", "beta <args>"},
                             [&](std::string_view arguments)
                             {
                                 executed_arguments = std::string(arguments);
                                 return CommandResult {true, "done"};
                             });

    CommandPaletteModel model;
    CommandPaletteController controller(model, manager, history);
    controller.open();

    ASSERT_TRUE(controller.handle_event(ftxui::Event::CtrlR));
    ASSERT_TRUE(controller.handle_event(ftxui::Event::Character("sec")));
    ASSERT_EQ(controller.model().matching_history_entries.size(), 1U);
    EXPECT_EQ(controller.model().matching_history_entries[0], "beta second");

    ASSERT_TRUE(controller.handle_event(ftxui::Event::Return));
    EXPECT_FALSE(controller.is_open());
    EXPECT_EQ(executed_arguments, "second");
    EXPECT_EQ(history.entries()[0], "beta second");

    remove_temp_settings_file(settings_path);
}

TEST(CommandPaletteControllerTest, TabCopiesSelectedHistoryCommandToQueryForEditing)
{
    const auto settings_path = make_temp_settings_path();
    SettingsStore settings_store(settings_path);
    CommandHistory history(settings_store);
    std::string error_message;
    ASSERT_TRUE(history.load(error_message));
    ASSERT_TRUE(history.record_command("beta first", error_message));
    ASSERT_TRUE(history.record_command("beta second", error_message));

    CommandManager manager;
    manager.register_command({"beta", "Second", "beta <args>"}, [](std::string_view) { return CommandResult {true, "done"}; });

    CommandPaletteModel model;
    CommandPaletteController controller(model, manager, history);
    controller.open();

    ASSERT_TRUE(controller.handle_event(ftxui::Event::CtrlR));
    ASSERT_TRUE(controller.handle_event(ftxui::Event::Character("sec")));
    ASSERT_EQ(controller.model().matching_history_entries.size(), 1U);
    EXPECT_EQ(controller.model().matching_history_entries[0], "beta second");

    ASSERT_TRUE(controller.handle_event(ftxui::Event::Tab));
    EXPECT_EQ(controller.model().mode, CommandPaletteMode::Commands);
    EXPECT_EQ(controller.model().query, "beta second");
    EXPECT_EQ(controller.model().cursor_position, std::string("beta second").size());

    remove_temp_settings_file(settings_path);
}

TEST(CommandPaletteControllerTest, HistoryModeReturnExecutesTypedQueryWhenNoHistoryMatches)
{
    const auto settings_path = make_temp_settings_path();
    SettingsStore settings_store(settings_path);
    CommandHistory history(settings_store);
    std::string error_message;
    ASSERT_TRUE(history.load(error_message));
    ASSERT_TRUE(history.record_command("alpha previous", error_message));

    CommandManager manager;
    std::string executed_arguments;
    manager.register_command({"beta", "Second", "beta <args>"},
                             [&](std::string_view arguments)
                             {
                                 executed_arguments = std::string(arguments);
                                 return CommandResult {true, "done"};
                             });

    CommandPaletteModel model;
    CommandPaletteController controller(model, manager, history);
    controller.open();

    ASSERT_TRUE(controller.handle_event(ftxui::Event::Character("beta fresh")));
    ASSERT_TRUE(controller.handle_event(ftxui::Event::CtrlR));
    ASSERT_TRUE(controller.model().matching_history_entries.empty());

    ASSERT_TRUE(controller.handle_event(ftxui::Event::Return));
    EXPECT_FALSE(controller.is_open());
    EXPECT_EQ(executed_arguments, "fresh");
    EXPECT_EQ(history.entries()[0], "beta fresh");

    remove_temp_settings_file(settings_path);
}

} // namespace slayerlog
