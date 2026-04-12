#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <exception>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "command_line_parser.hpp"
#include "commands/command_history.hpp"
#include "command_palette_controller.hpp"
#include "command_palette_model.hpp"
#include "command_manager.hpp"
#include "command_palette_view.hpp"
#include "debug_log.hpp"
#include "log_controller.hpp"
#include "log_view.hpp"
#include "master_controller.hpp"
#include "master_view.hpp"
#include "log_model.hpp"
#include "settings_store.hpp"
#include "tracked_source_manager.hpp"

namespace
{

/**
 * @brief Joins the source labels into the header text shown in the UI.
 */
std::string build_header_text(const std::vector<std::string>& labels)
{
    if (labels.empty())
    {
        return "No files opened (use open-file <path>)";
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

std::optional<int> highest_shown_line_number(const slayerlog::LogModel& model, const slayerlog::LogController& controller, int viewport_line_count)
{
    if (model.line_count() == 0)
    {
        return std::nullopt;
    }

    const int clamped_viewport_line_count = std::max(1, viewport_line_count);
    const int first_visible_line_index    = controller.first_visible_line_index(model, clamped_viewport_line_count).value;
    const int last_visible_line_index     = std::min(model.line_count() - 1, first_visible_line_index + clamped_viewport_line_count - 1);
    return model.line_number_for_visible_line(slayerlog::VisibleLineIndex {last_visible_line_index});
}

void reload_model_from_manager(const slayerlog::TrackedSourceManager& tracked_source_manager, std::string& header_text, slayerlog::LogModel& model, ftxui::ScreenInteractive& screen)
{
    header_text = build_header_text(tracked_source_manager.source_labels());
    model.set_show_source_labels(tracked_source_manager.source_count() > 1);
    model.replace_batch(tracked_source_manager.snapshot());
    screen.PostEvent(ftxui::Event::Custom);
}

slayerlog::CommandResult open_file_command(std::string_view file_path, slayerlog::TrackedSourceManager& tracked_source_manager, std::string& header_text, slayerlog::LogModel& model, ftxui::ScreenInteractive& screen)
{
    const auto error = tracked_source_manager.open_source(file_path);
    if (error.has_value())
    {
        SLAYERLOG_LOG_ERROR("open-file failed file=" << file_path << " error=" << *error);
        return slayerlog::CommandResult {false, *error};
    }

    reload_model_from_manager(tracked_source_manager, header_text, model, screen);
    return slayerlog::CommandResult {true, "Opened file: " + std::string(file_path)};
}

slayerlog::CommandResult close_open_file_command(slayerlog::CommandPaletteController& command_palette_controller, slayerlog::TrackedSourceManager& tracked_source_manager, std::string& header_text, slayerlog::LogModel& model,
                                                 ftxui::ScreenInteractive& screen)
{
    const auto labels = tracked_source_manager.source_labels();
    if (labels.empty())
    {
        return slayerlog::CommandResult {false, "No open files to close"};
    }

    command_palette_controller.open_close_open_file_picker(labels,
                                                           [&](std::size_t selected_index) -> slayerlog::CommandResult
                                                           {
                                                               std::string closed_label;
                                                               const auto error = tracked_source_manager.close_source(selected_index, &closed_label);
                                                               if (error.has_value())
                                                               {
                                                                   SLAYERLOG_LOG_ERROR("close-open-file failed selected_index=" << selected_index << " error=" << *error);
                                                                   return slayerlog::CommandResult {false, *error};
                                                               }

                                                               reload_model_from_manager(tracked_source_manager, header_text, model, screen);
                                                               return slayerlog::CommandResult {true, "Closed file: " + closed_label};
                                                           });

    return slayerlog::CommandResult {true, "Select a file to close", false};
}

void append_batch_to_model(const slayerlog::LogBatch& batch, slayerlog::LogModel& model, ftxui::ScreenInteractive& screen)
{
    if (batch.empty())
    {
        return;
    }

    model.append_batch(batch);
    screen.PostEvent(ftxui::Event::Custom);
}

std::thread start_watcher_thread(int poll_interval_ms, slayerlog::TrackedSourceManager& tracked_source_manager, std::mutex& model_mutex, slayerlog::LogModel& model, ftxui::ScreenInteractive& screen, std::atomic<bool>& keep_running)
{
    return std::thread(
        [poll_interval_ms, tracked_source_manager = &tracked_source_manager, model_mutex = &model_mutex, model = &model, screen = &screen, keep_running = &keep_running]
        {
            while (*keep_running)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
                if (!*keep_running)
                {
                    break;
                }

                std::lock_guard lock(*model_mutex);
                append_batch_to_model(tracked_source_manager->poll(), *model, *screen);
            }
        });
}

void register_commands(slayerlog::CommandManager& command_manager, slayerlog::LogModel& model, slayerlog::LogController& controller, std::function<int()> viewport_line_count, slayerlog::CommandPaletteController& command_palette_controller,
                       std::string& header_text, ftxui::ScreenInteractive& screen, slayerlog::TrackedSourceManager& tracked_source_manager)
{
    command_manager.register_command({"filter-in", "Show lines matching text or regex", "filter-in <text|re:regex>"},
                                     [&](std::string_view arguments)
                                     {
                                         if (arguments.empty())
                                         {
                                             return slayerlog::CommandResult {false, "Usage: filter-in <text|re:regex>"};
                                         }

                                         try
                                         {
                                             model.add_include_filter(std::string(arguments));
                                         }
                                         catch (const std::invalid_argument& error)
                                         {
                                             return slayerlog::CommandResult {false, "Invalid filter-in pattern: " + std::string(error.what())};
                                         }

                                         return slayerlog::CommandResult {true, "Added include filter: " + std::string(arguments)};
                                     });

    command_manager.register_command({"filter-out", "Hide lines matching text or regex", "filter-out <text|re:regex>"},
                                     [&](std::string_view arguments)
                                     {
                                         if (arguments.empty())
                                         {
                                             return slayerlog::CommandResult {false, "Usage: filter-out <text|re:regex>"};
                                         }

                                         try
                                         {
                                             model.add_exclude_filter(std::string(arguments));
                                         }
                                         catch (const std::invalid_argument& error)
                                         {
                                             return slayerlog::CommandResult {false, "Invalid filter-out pattern: " + std::string(error.what())};
                                         }

                                         return slayerlog::CommandResult {true, "Added exclude filter: " + std::string(arguments)};
                                     });

    command_manager.register_command({"reset-filters", "Clear all active filters", "reset-filters"},
                                     [&](std::string_view arguments)
                                     {
                                         if (!arguments.empty())
                                         {
                                             return slayerlog::CommandResult {false, "Usage: reset-filters"};
                                         }

                                         model.reset_filters();
                                         return slayerlog::CommandResult {true, "Cleared all filters"};
                                     });

    command_manager.register_command({"clear-filters", "Alias for reset-filters", "clear-filters"},
                                     [&](std::string_view arguments)
                                     {
                                         if (!arguments.empty())
                                         {
                                             return slayerlog::CommandResult {false, "Usage: clear-filters"};
                                         }

                                         model.reset_filters();
                                         return slayerlog::CommandResult {true, "Cleared all filters"};
                                     });

    command_manager.register_command({"hide-columns", "Hide displayed columns in a range", "hide-columns <xx-yy>"},
                                     [&](std::string_view arguments)
                                     {
                                         const auto hidden_columns = slayerlog::parse_hidden_column_range(arguments);
                                         if (!hidden_columns.has_value())
                                         {
                                             return slayerlog::CommandResult {false, "Usage: hide-columns <xx-yy>"};
                                         }

                                         model.hide_columns(hidden_columns->start, hidden_columns->end);
                                         return slayerlog::CommandResult {
                                             true,
                                             "Hidden columns " + std::to_string(hidden_columns->start) + "-" + std::to_string(hidden_columns->end),
                                         };
                                     });

    command_manager.register_command({"reset-column-filter", "Clear the hidden column range", "reset-column-filter"},
                                     [&](std::string_view arguments)
                                     {
                                         if (!arguments.empty())
                                         {
                                             return slayerlog::CommandResult {false, "Usage: reset-column-filter"};
                                         }

                                         model.reset_hidden_columns();
                                         return slayerlog::CommandResult {true, "Cleared hidden column filter"};
                                     });

    command_manager.register_command({"clear-column-filters", "Alias for reset-column-filter", "clear-column-filters"},
                                     [&](std::string_view arguments)
                                     {
                                         if (!arguments.empty())
                                         {
                                             return slayerlog::CommandResult {false, "Usage: clear-column-filters"};
                                         }

                                         model.reset_hidden_columns();
                                         return slayerlog::CommandResult {true, "Cleared hidden column filter"};
                                     });

    command_manager.register_command({"open-file", "Open file and reload all tracked logs", "open-file <path>"},
                                     [&](std::string_view arguments)
                                     {
                                         const std::string file_path = trim_text(arguments);
                                         if (file_path.empty())
                                         {
                                             return slayerlog::CommandResult {false, "Usage: open-file <path>"};
                                         }

                                         return open_file_command(file_path, tracked_source_manager, header_text, model, screen);
                                     });

    command_manager.register_command({"close-open-file", "Close one currently open file", "close-open-file"},
                                     [&](std::string_view arguments)
                                     {
                                         if (!trim_text(arguments).empty())
                                         {
                                             return slayerlog::CommandResult {false, "Usage: close-open-file"};
                                         }

                                         return close_open_file_command(command_palette_controller, tracked_source_manager, header_text, model, screen);
                                     });

    command_manager.register_command({"go-to-line", "Center the view on a line number", "go-to-line <line-number>"},
                                     [&, viewport_line_count](std::string_view arguments)
                                     {
                                         const auto line_number = parse_positive_line_number(arguments);
                                         if (!line_number.has_value())
                                         {
                                             return slayerlog::CommandResult {false, "Usage: go-to-line <line-number>"};
                                         }

                                         if (*line_number > model.total_line_count())
                                         {
                                             return slayerlog::CommandResult {false, "Line " + std::to_string(*line_number) + " is out of range"};
                                         }

                                         if (!controller.go_to_line(model, *line_number, viewport_line_count()))
                                         {
                                             return slayerlog::CommandResult {
                                                 false,
                                                 "Line " + std::to_string(*line_number) + " is hidden by current filters or line cutoff",
                                             };
                                         }

                                         return slayerlog::CommandResult {
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
                                             return slayerlog::CommandResult {false, "Usage: hide-before-line <line-number>"};
                                         }

                                         model.hide_before_line_number(*line_number);
                                         if (*line_number == 1)
                                         {
                                             return slayerlog::CommandResult {true, "Showing all lines"};
                                         }

                                         return slayerlog::CommandResult {
                                             true,
                                             "Hidden all lines before line " + std::to_string(*line_number),
                                         };
                                     });

    command_manager.register_command({"hide-shown-lines", "Hide all currently shown lines", "hide-shown-lines"},
                                     [&, viewport_line_count](std::string_view arguments)
                                     {
                                         if (!trim_text(arguments).empty())
                                         {
                                             return slayerlog::CommandResult {false, "Usage: hide-shown-lines"};
                                         }

                                         const auto line_number = highest_shown_line_number(model, controller, viewport_line_count());
                                         if (!line_number.has_value())
                                         {
                                             return slayerlog::CommandResult {false, "No lines are currently shown"};
                                         }

                                         model.hide_before_line_number(*line_number + 1);
                                         return slayerlog::CommandResult {true, "Hidden all currently shown lines"};
                                     });

    command_manager.register_command({"find", "Find lines matching text or regex", "find <text|re:regex>"},
                                     [&, viewport_line_count](std::string_view arguments)
                                     {
                                         const std::string query = trim_text(arguments);
                                         if (query.empty())
                                         {
                                             return slayerlog::CommandResult {false, "Usage: find <text|re:regex>"};
                                         }

                                         bool focused_visible_match = false;
                                         try
                                         {
                                             focused_visible_match = controller.set_find_query(model, query, viewport_line_count());
                                         }
                                         catch (const std::invalid_argument& error)
                                         {
                                             return slayerlog::CommandResult {false, "Invalid find pattern: " + std::string(error.what())};
                                         }

                                         const int visible_matches = model.visible_find_match_count();
                                         const int total_matches   = model.total_find_match_count();
                                         if (focused_visible_match)
                                         {
                                             return slayerlog::CommandResult {
                                                 true,
                                                 "Find active: " + model.find_query() + " (" + std::to_string(visible_matches) + " visible / " + std::to_string(total_matches) + " total)",
                                             };
                                         }

                                         return slayerlog::CommandResult {
                                             true,
                                             "Find active: " + model.find_query() + " (0 visible / " + std::to_string(total_matches) + " total)",
                                         };
                                     });
}

} // namespace

int main(int argc, char** argv)
{
    slayerlog::TrackedSourceManager tracked_source_manager;
    slayerlog::debug_log::initialize(argc > 0 ? argv[0] : nullptr);
    SLAYERLOG_LOG_INFO("Debug log initialized at " << slayerlog::debug_log::log_file_path().string());

    const auto config       = slayerlog::parse_command_line(argc, argv);
    std::string header_text = build_header_text({});
    SLAYERLOG_LOG_INFO("Starting slayerlog poll_interval_ms=" << config.poll_interval_ms << " configured_sources=" << config.file_paths.size());
    for (const auto& file_path : config.file_paths)
    {
        const auto error = tracked_source_manager.open_source(file_path);
        if (error.has_value())
        {
            SLAYERLOG_LOG_ERROR("Initial source open failed file=" << file_path << " error=" << *error);
            std::cerr << *error << '\n';
            return 1;
        }
    }

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    screen.TrackMouse();

    std::mutex model_mutex;
    slayerlog::LogModel model;
    model.set_show_source_labels(tracked_source_manager.source_count() > 1);

    slayerlog::SettingsStore settings_store(slayerlog::default_settings_file_path());
    slayerlog::CommandHistory command_history(settings_store);
    std::string settings_error_message;
    if (!command_history.load(settings_error_message))
    {
        SLAYERLOG_LOG_WARNING("Failed to load settings from " << settings_store.file_path() << ": " << settings_error_message);
    }

    slayerlog::CommandPaletteModel command_palette_model;
    slayerlog::CommandManager command_manager;
    slayerlog::LogView view;
    slayerlog::CommandPaletteView command_palette_view;
    slayerlog::MasterView master_view(view, command_palette_view);
    slayerlog::LogController controller;

    slayerlog::CommandPaletteController command_palette_controller(command_palette_model, command_manager, command_history);

    register_commands(command_manager, model, controller, [&] { return view.visible_line_count(screen.dimy()); }, command_palette_controller, header_text, screen, tracked_source_manager);

    slayerlog::MasterController master_controller(model, controller, view, screen, command_palette_controller);

    {
        std::lock_guard lock(model_mutex);
        reload_model_from_manager(tracked_source_manager, header_text, model, screen);
    }

    std::atomic<bool> keep_running = true;
    std::thread watcher_thread     = start_watcher_thread(config.poll_interval_ms, tracked_source_manager, model_mutex, model, screen, keep_running);

    auto viewer = ftxui::Renderer(
        [&]
        {
            std::lock_guard lock(model_mutex);
            return master_view.render(model, controller, header_text, screen.dimy(), command_palette_controller.model());
        });

    viewer |= ftxui::CatchEvent(
        [&](ftxui::Event event)
        {
            std::lock_guard lock(model_mutex);
            return master_controller.handle_event(event);
        });

    screen.Loop(viewer);
    SLAYERLOG_LOG_INFO("Screen loop exited");
    keep_running = false;
    if (watcher_thread.joinable())
    {
        watcher_thread.join();
    }

    SLAYERLOG_LOG_INFO("Slayerlog shutdown complete");

    return 0;
}
