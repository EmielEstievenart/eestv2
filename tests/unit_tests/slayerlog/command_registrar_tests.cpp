#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <ftxui/component/screen_interactive.hpp>

#include "command_palette_controller.hpp"
#include "command_palette_model.hpp"
#include "command_registrar.hpp"
#include "command_manager.hpp"
#include "log_controller.hpp"
#include "tracked_sources/all_processed_sources.hpp"
#include "tracked_sources/all_tracked_sources.hpp"

namespace slayerlog
{

namespace
{

std::filesystem::path make_temp_export_path()
{
    const auto unique_suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    return std::filesystem::temp_directory_path() / ("slayerlog_visible_export_" + unique_suffix + ".txt");
}

void remove_temp_export_file(const std::filesystem::path& export_path)
{
    std::error_code error_code;
    std::filesystem::remove(export_path, error_code);
}

std::string read_file_contents(const std::filesystem::path& export_path)
{
    std::ifstream input(export_path, std::ios::binary);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

std::string render_all_visible_lines(const AllProcessedSources& processed_sources)
{
    std::ostringstream output;
    for (int line_index = 0; line_index < processed_sources.line_count(); ++line_index)
    {
        if (line_index > 0)
        {
            output << '\n';
        }

        output << processed_sources.rendered_line(line_index);
    }

    return output.str();
}

} // namespace

TEST(CommandRegistrarTest, ExportVisibleTextWritesAllVisibleRenderedLines)
{
    AllProcessedSources processed_sources;
    processed_sources.append_lines({
        ObservedLogLine {"alpha.log", "keep first"},
        ObservedLogLine {"alpha.log", "drop second"},
        ObservedLogLine {"alpha.log", "keep third"},
    });
    processed_sources.add_include_filter("keep");
    processed_sources.hide_columns(2, 5);
    ASSERT_EQ(processed_sources.line_count(), 2);

    CommandManager command_manager;
    LogController controller;
    CommandPaletteModel command_palette_model;
    CommandPaletteController command_palette_controller(command_palette_model, command_manager);
    std::string header_text;
    auto screen = ftxui::ScreenInteractive::FixedSize(80, 24);
    AllTrackedSources tracked_sources;
    register_commands(command_manager, processed_sources, controller, command_palette_controller, header_text, screen, tracked_sources);

    const auto export_path = make_temp_export_path();
    const auto result      = command_manager.execute("export-visible-text " + export_path.string());

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.message, "Exported 2 visible lines to " + export_path.string());
    EXPECT_EQ(read_file_contents(export_path), render_all_visible_lines(processed_sources));

    remove_temp_export_file(export_path);
}

TEST(CommandRegistrarTest, DeleteFiltersCommandOpensPickerAndRemovesSelectedFilters)
{
    AllProcessedSources processed_sources;
    processed_sources.append_lines({
        ObservedLogLine {"alpha.log", "show keep alpha"},
        ObservedLogLine {"alpha.log", "hide beta"},
        ObservedLogLine {"alpha.log", "show gamma"},
    });
    processed_sources.add_include_filter("show");
    processed_sources.add_include_filter("gamma");
    processed_sources.add_exclude_filter("beta");

    CommandManager command_manager;
    LogController controller;
    CommandPaletteModel command_palette_model;
    CommandPaletteController command_palette_controller(command_palette_model, command_manager);
    std::string header_text;
    auto screen = ftxui::ScreenInteractive::FixedSize(80, 24);
    AllTrackedSources tracked_sources;
    register_commands(command_manager, processed_sources, controller, command_palette_controller, header_text, screen, tracked_sources);

    const auto result = command_manager.execute("delete-filters");

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.close_palette_on_success);
    EXPECT_EQ(result.message, "Mark filters to delete and press Enter");
    ASSERT_TRUE(command_palette_controller.is_open());
    EXPECT_EQ(command_palette_controller.model().mode, CommandPaletteMode::DeleteFilters);
    ASSERT_EQ(command_palette_controller.model().filter_picker_entries.size(), 3U);
    EXPECT_EQ(command_palette_controller.model().filter_picker_entries[0].label, "show");
    EXPECT_EQ(command_palette_controller.model().filter_picker_entries[1].label, "gamma");
    EXPECT_EQ(command_palette_controller.model().filter_picker_entries[2].label, "beta");
    EXPECT_TRUE(command_palette_controller.model().filter_picker_entries[0].include);
    EXPECT_TRUE(command_palette_controller.model().filter_picker_entries[1].include);
    EXPECT_FALSE(command_palette_controller.model().filter_picker_entries[2].include);

    ASSERT_TRUE(command_palette_controller.handle_event(ftxui::Event::Character(" ")));
    ASSERT_TRUE(command_palette_controller.handle_event(ftxui::Event::ArrowDown));
    ASSERT_TRUE(command_palette_controller.handle_event(ftxui::Event::ArrowDown));
    ASSERT_TRUE(command_palette_controller.handle_event(ftxui::Event::Character(" ")));
    ASSERT_TRUE(command_palette_controller.handle_event(ftxui::Event::Return));

    EXPECT_FALSE(command_palette_controller.is_open());
    EXPECT_EQ(processed_sources.include_filters().size(), 1U);
    EXPECT_EQ(processed_sources.include_filters()[0], "gamma");
    EXPECT_TRUE(processed_sources.exclude_filters().empty());
}

TEST(CommandRegistrarTest, DeleteFiltersCommandFailsWhenNoFiltersExist)
{
    AllProcessedSources processed_sources;

    CommandManager command_manager;
    LogController controller;
    CommandPaletteModel command_palette_model;
    CommandPaletteController command_palette_controller(command_palette_model, command_manager);
    std::string header_text;
    auto screen = ftxui::ScreenInteractive::FixedSize(80, 24);
    AllTrackedSources tracked_sources;
    register_commands(command_manager, processed_sources, controller, command_palette_controller, header_text, screen, tracked_sources);

    const auto result = command_manager.execute("delete-filters");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.message, "No filters to delete");
}

} // namespace slayerlog
