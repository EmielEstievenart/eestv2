#include "command_registrar.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include <ftxui/component/screen_interactive.hpp>

#include "command_manager.hpp"
#include "command_palette_controller.hpp"
#include "debug_log.hpp"
#include "all_tracked_sources.hpp"
#include "log_controller.hpp"
#include "log_source.hpp"
#include "processed_sources.hpp"

namespace slayerlog
{

std::string build_header_text(const std::vector<std::string>& labels)
{
    if (labels.empty())
    {
        return "No files opened (use open-file <path> or open-folder <path>)";
    }

    std::ostringstream output;
    for (std::size_t index = 0; index < labels.size(); ++index)
    {
        if (index > 0)
        {
            output << " | ";
        }

        output << labels[index];
    }

    return output.str();
}

void reload_processed_sources(const AllTrackedSources& tracked_sources, std::string& header_text, ProcessedSources& processed_sources, LogController& controller, ftxui::ScreenInteractive& screen)
{
    header_text = build_header_text(tracked_sources.source_labels());
    processed_sources.set_show_source_labels(tracked_sources.source_count() > 1);
    processed_sources.rebuild_from_sources(tracked_sources);
    controller.rebuild_view(processed_sources);
    screen.PostEvent(ftxui::Event::Custom);
}

namespace
{

std::optional<int> parse_positive_line_number(std::string_view text)
{
    if (text.empty())
    {
        return std::nullopt;
    }

    const std::string line_text(text);
    std::size_t parsed_length = 0;
    int line_number           = 0;
    try
    {
        line_number = std::stoi(line_text, &parsed_length);
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }

    if (parsed_length != line_text.size() || line_number <= 0)
    {
        return std::nullopt;
    }

    return line_number;
}

std::string trim_text(std::string_view text)
{
    std::size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0)
    {
        ++start;
    }

    std::size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
    {
        --end;
    }

    return std::string(text.substr(start, end - start));
}

std::optional<int> highest_shown_line_number(const ProcessedSources& processed_sources, const LogController& controller)
{
    if (processed_sources.line_count() == 0)
    {
        return std::nullopt;
    }

    const int viewport_line_count = std::max(1, controller.text_view_controller().viewport_line_count());
    const int first_visible_line  = controller.text_view_controller().first_visible_line();
    const int last_visible_line   = std::min(processed_sources.line_count() - 1, first_visible_line + viewport_line_count - 1);
    return processed_sources.line_number_for_visible_line(VisibleLineIndex {last_visible_line});
}

CommandResult open_file_command(std::string_view file_path, AllTrackedSources& tracked_sources, std::string& header_text, ProcessedSources& processed_sources, LogController& controller, ftxui::ScreenInteractive& screen)
{
    const auto error = tracked_sources.open_source(file_path);
    if (error.has_value())
    {
        SLAYERLOG_LOG_ERROR("open-file failed file=" << file_path << " error=" << *error);
        return CommandResult {false, *error};
    }

    reload_processed_sources(tracked_sources, header_text, processed_sources, controller, screen);
    return CommandResult {true, "Opened file: " + std::string(file_path)};
}

CommandResult open_folder_command(std::string_view folder_path, AllTrackedSources& tracked_sources, std::string& header_text, ProcessedSources& processed_sources, LogController& controller, ftxui::ScreenInteractive& screen)
{
    LogSource source;
    try
    {
        source = make_local_folder_source(folder_path);
    }
    catch (const std::exception& ex)
    {
        return CommandResult {false, ex.what()};
    }

    const auto error = tracked_sources.open_source(source);
    if (error.has_value())
    {
        SLAYERLOG_LOG_ERROR("open-folder failed folder=" << source_display_path(source) << " error=" << *error);
        return CommandResult {false, *error};
    }

    reload_processed_sources(tracked_sources, header_text, processed_sources, controller, screen);
    return CommandResult {true, "Opened folder: " + source_display_path(source)};
}

CommandResult close_open_file_command(CommandPaletteController& command_palette_controller, AllTrackedSources& tracked_sources, std::string& header_text, ProcessedSources& processed_sources, LogController& controller,
                                      ftxui::ScreenInteractive& screen)
{
    const auto labels = tracked_sources.source_labels();
    if (labels.empty())
    {
        return CommandResult {false, "No open files to close"};
    }

    command_palette_controller.open_close_open_file_picker(labels,
                                                           [&](std::size_t selected_index) -> CommandResult
                                                           {
                                                               std::string closed_label;
                                                               const auto error = tracked_sources.close_source(selected_index, &closed_label);
                                                               if (error.has_value())
                                                               {
                                                                   SLAYERLOG_LOG_ERROR("close-open-file failed selected_index=" << selected_index << " error=" << *error);
                                                                   return CommandResult {false, *error};
                                                               }

                                                               reload_processed_sources(tracked_sources, header_text, processed_sources, controller, screen);
                                                               return CommandResult {true, "Closed file: " + closed_label};
                                                           });

    return CommandResult {true, "Select a file to close", false};
}

} // namespace

void register_commands(CommandManager& command_manager, ProcessedSources& processed_sources, LogController& controller, CommandPaletteController& command_palette_controller, std::string& header_text, ftxui::ScreenInteractive& screen,
                       AllTrackedSources& tracked_sources)
{
    command_manager.register_command({"filter-in", "Show lines matching text or regex", "filter-in <text|re:regex>"},
                                     [&](std::string_view arguments)
                                     {
                                         if (arguments.empty())
                                         {
                                             return CommandResult {false, "Usage: filter-in <text|re:regex>"};
                                         }

                                         try
                                         {
                                             processed_sources.add_include_filter(std::string(arguments));
                                         }
                                         catch (const std::invalid_argument& error)
                                         {
                                             return CommandResult {false, "Invalid filter-in pattern: " + std::string(error.what())};
                                         }

                                         controller.rebuild_view(processed_sources);
                                         return CommandResult {true, "Added include filter: " + std::string(arguments)};
                                     });

    command_manager.register_command({"filter-out", "Hide lines matching text or regex", "filter-out <text|re:regex>"},
                                     [&](std::string_view arguments)
                                     {
                                         if (arguments.empty())
                                         {
                                             return CommandResult {false, "Usage: filter-out <text|re:regex>"};
                                         }

                                         try
                                         {
                                             processed_sources.add_exclude_filter(std::string(arguments));
                                         }
                                         catch (const std::invalid_argument& error)
                                         {
                                             return CommandResult {false, "Invalid filter-out pattern: " + std::string(error.what())};
                                         }

                                         controller.rebuild_view(processed_sources);
                                         return CommandResult {true, "Added exclude filter: " + std::string(arguments)};
                                     });

    command_manager.register_command({"reset-filters", "Clear all active filters", "reset-filters"},
                                     [&](std::string_view arguments)
                                     {
                                         if (!arguments.empty())
                                         {
                                             return CommandResult {false, "Usage: reset-filters"};
                                         }

                                         processed_sources.reset_filters();
                                         controller.rebuild_view(processed_sources);
                                         return CommandResult {true, "Cleared all filters"};
                                     });

    command_manager.register_command({"clear-filters", "Alias for reset-filters", "clear-filters"},
                                     [&](std::string_view arguments)
                                     {
                                         if (!arguments.empty())
                                         {
                                             return CommandResult {false, "Usage: clear-filters"};
                                         }

                                         processed_sources.reset_filters();
                                         controller.rebuild_view(processed_sources);
                                         return CommandResult {true, "Cleared all filters"};
                                     });

    command_manager.register_command({"hide-columns", "Hide displayed columns in a range", "hide-columns <xx-yy>"},
                                     [&](std::string_view arguments)
                                     {
                                         const auto hidden_columns = parse_hidden_column_range(arguments);
                                         if (!hidden_columns.has_value())
                                         {
                                             return CommandResult {false, "Usage: hide-columns <xx-yy>"};
                                         }

                                         processed_sources.hide_columns(hidden_columns->start, hidden_columns->end);
                                         controller.rebuild_view(processed_sources);
                                         return CommandResult {
                                             true,
                                             "Hidden columns " + std::to_string(hidden_columns->start) + "-" + std::to_string(hidden_columns->end),
                                         };
                                     });

    command_manager.register_command({"reset-column-filter", "Clear the hidden column range", "reset-column-filter"},
                                     [&](std::string_view arguments)
                                     {
                                         if (!arguments.empty())
                                         {
                                             return CommandResult {false, "Usage: reset-column-filter"};
                                         }

                                         processed_sources.reset_hidden_columns();
                                         controller.rebuild_view(processed_sources);
                                         return CommandResult {true, "Cleared hidden column filter"};
                                     });

    command_manager.register_command({"clear-column-filters", "Alias for reset-column-filter", "clear-column-filters"},
                                     [&](std::string_view arguments)
                                     {
                                         if (!arguments.empty())
                                         {
                                             return CommandResult {false, "Usage: clear-column-filters"};
                                         }

                                         processed_sources.reset_hidden_columns();
                                         controller.rebuild_view(processed_sources);
                                         return CommandResult {true, "Cleared hidden column filter"};
                                     });

    command_manager.register_command({"open-file", "Open file and reload all tracked logs", "open-file <path>"},
                                     [&](std::string_view arguments)
                                     {
                                         const std::string file_path = trim_text(arguments);
                                         if (file_path.empty())
                                         {
                                             return CommandResult {false, "Usage: open-file <path>"};
                                         }

                                         return open_file_command(file_path, tracked_sources, header_text, processed_sources, controller, screen);
                                     });

    command_manager.register_command({"open-folder", "Open folder and reload all tracked logs", "open-folder <path>"},
                                     [&](std::string_view arguments)
                                     {
                                         const std::string folder_path = trim_text(arguments);
                                         if (folder_path.empty())
                                         {
                                             return CommandResult {false, "Usage: open-folder <path>"};
                                         }

                                         return open_folder_command(folder_path, tracked_sources, header_text, processed_sources, controller, screen);
                                     });

    command_manager.register_command({"close-open-file", "Close one currently open file", "close-open-file"},
                                     [&](std::string_view arguments)
                                     {
                                         if (!trim_text(arguments).empty())
                                         {
                                             return CommandResult {false, "Usage: close-open-file"};
                                         }

                                         return close_open_file_command(command_palette_controller, tracked_sources, header_text, processed_sources, controller, screen);
                                     });

    command_manager.register_command({"go-to-line", "Center the view on a line number", "go-to-line <line-number>"},
                                     [&](std::string_view arguments)
                                     {
                                         const auto line_number = parse_positive_line_number(arguments);
                                         if (!line_number.has_value())
                                         {
                                             return CommandResult {false, "Usage: go-to-line <line-number>"};
                                         }

                                         if (*line_number > processed_sources.total_line_count())
                                         {
                                             return CommandResult {false, "Line " + std::to_string(*line_number) + " is out of range"};
                                         }

                                         if (!controller.go_to_line(processed_sources, *line_number))
                                         {
                                             return CommandResult {
                                                 false,
                                                 "Line " + std::to_string(*line_number) + " is hidden by current filters or line cutoff",
                                             };
                                         }

                                         return CommandResult {
                                             true,
                                             "Centered view on line " + std::to_string(*line_number),
                                         };
                                     });

    command_manager.register_command({"hide-before-line", "Hide all raw lines before a line number", "hide-before-line <line-number>"},
                                     [&](std::string_view arguments)
                                     {
                                         const auto line_number = parse_positive_line_number(arguments);
                                         if (!line_number.has_value())
                                         {
                                             return CommandResult {false, "Usage: hide-before-line <line-number>"};
                                         }

                                         processed_sources.hide_before_line_number(*line_number);
                                         controller.rebuild_view(processed_sources);
                                         if (*line_number == 1)
                                         {
                                             return CommandResult {true, "Showing all lines"};
                                         }

                                         return CommandResult {
                                             true,
                                             "Hidden all lines before line " + std::to_string(*line_number),
                                         };
                                     });

    command_manager.register_command({"hide-shown-lines", "Hide all currently shown lines", "hide-shown-lines"},
                                     [&](std::string_view arguments)
                                     {
                                         if (!trim_text(arguments).empty())
                                         {
                                             return CommandResult {false, "Usage: hide-shown-lines"};
                                         }

                                         const auto line_number = highest_shown_line_number(processed_sources, controller);
                                         if (!line_number.has_value())
                                         {
                                             return CommandResult {false, "No lines are currently shown"};
                                         }

                                         processed_sources.hide_before_line_number(*line_number + 1);
                                         controller.rebuild_view(processed_sources);
                                         return CommandResult {true, "Hidden all currently shown lines"};
                                     });

    command_manager.register_command({"find", "Find lines matching text or regex", "find <text|re:regex>"},
                                     [&](std::string_view arguments)
                                     {
                                         const std::string query = trim_text(arguments);
                                         if (query.empty())
                                         {
                                             return CommandResult {false, "Usage: find <text|re:regex>"};
                                         }

                                         bool focused_visible_match = false;
                                         try
                                         {
                                             focused_visible_match = controller.set_find_query(processed_sources, query);
                                         }
                                         catch (const std::invalid_argument& error)
                                         {
                                             return CommandResult {false, "Invalid find pattern: " + std::string(error.what())};
                                         }

                                         const int visible_matches = controller.visible_find_match_count(processed_sources);
                                         const int total_matches   = controller.total_find_match_count();
                                         if (focused_visible_match)
                                         {
                                             return CommandResult {
                                                 true,
                                                 "Find active: " + controller.find_query() + " (" + std::to_string(visible_matches) + " visible / " + std::to_string(total_matches) + " total)",
                                             };
                                         }

                                         return CommandResult {
                                             true,
                                             "Find active: " + controller.find_query() + " (0 visible / " + std::to_string(total_matches) + " total)",
                                         };
                                     });
}

} // namespace slayerlog
