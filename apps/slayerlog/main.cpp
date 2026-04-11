#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
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
#include "command_history.hpp"
#include "command_palette_controller.hpp"
#include "command_palette_model.hpp"
#include "command_manager.hpp"
#include "command_palette_view.hpp"
#include "debug_log.hpp"
#include "watchers/file_watcher.hpp"
#include "log_batch.hpp"
#include "log_controller.hpp"
#include "log_source.hpp"
#include "log_view.hpp"
#include "log_watcher.hpp"
#include "master_controller.hpp"
#include "master_view.hpp"
#include "watchers/ssh_tail_watcher.hpp"
#include "log_model.hpp"
#include "settings_store.hpp"

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

std::vector<slayerlog::LogSource> parse_log_sources(const std::vector<std::string>& specs)
{
    std::vector<slayerlog::LogSource> sources;
    sources.reserve(specs.size());
    for (const std::string& spec : specs)
    {
        sources.push_back(slayerlog::parse_log_source(spec));
    }

    return sources;
}

bool contains_tracked_source(const std::vector<slayerlog::LogSource>& tracked_sources, const slayerlog::LogSource& candidate_source)
{
    return std::any_of(tracked_sources.begin(), tracked_sources.end(), [&](const slayerlog::LogSource& tracked_source) { return slayerlog::same_source(tracked_source, candidate_source); });
}

struct WatchedFile
{
    slayerlog::LogSource source;
    std::string source_label;
    std::unique_ptr<slayerlog::LogWatcher> watcher;
};

std::unique_ptr<slayerlog::LogWatcher> create_watcher_for_source(const slayerlog::LogSource& source)
{
    if (source.kind == slayerlog::LogSourceKind::SshRemoteFile)
    {
        return std::make_unique<slayerlog::SshTailWatcher>(source);
    }

    return std::make_unique<slayerlog::FileWatcher>(source.local_path);
}

std::vector<WatchedFile> create_file_watchers(const std::vector<slayerlog::LogSource>& sources, const std::vector<std::string>& source_labels)
{
    std::vector<WatchedFile> watched_files;
    watched_files.reserve(sources.size());

    for (std::size_t index = 0; index < sources.size(); ++index)
    {
        watched_files.push_back(WatchedFile {
            sources[index],
            source_labels[index],
            create_watcher_for_source(sources[index]),
        });
    }

    return watched_files;
}

std::vector<slayerlog::WatcherLineBatch> collect_watcher_batches(std::vector<WatchedFile>& watched_files)
{
    std::vector<slayerlog::WatcherLineBatch> watcher_batches;
    watcher_batches.reserve(watched_files.size());

    for (auto& watched_file : watched_files)
    {
        slayerlog::WatcherLineBatch watcher_batch;
        watched_file.watcher->poll(watcher_batch);
        SLAYERLOG_LOG_TRACE("Initial poll source=" << slayerlog::source_display_path(watched_file.source) << " returned_lines=" << watcher_batch.size());
        watcher_batches.push_back(std::move(watcher_batch));
    }

    return watcher_batches;
}

void append_batch_to_model(const std::vector<slayerlog::WatcherLineBatch>& watcher_batches, const std::vector<std::string>& source_labels, slayerlog::LogModel& model, ftxui::ScreenInteractive& screen)
{
    const auto merged_lines = slayerlog::merge_log_batch(watcher_batches, source_labels);
    SLAYERLOG_LOG_TRACE("Merging watcher batches watcher_count=" << watcher_batches.size() << " merged_lines=" << merged_lines.size());
    if (merged_lines.empty())
    {
        return;
    }

    model.append_lines(merged_lines);

    screen.PostEvent(ftxui::Event::Custom);
}

std::thread start_watcher_thread(int poll_interval_ms, std::vector<WatchedFile>& watched_files, const std::vector<std::string>& source_labels, std::mutex& model_mutex, slayerlog::LogModel& model, ftxui::ScreenInteractive& screen,
                                 std::atomic<bool>& keep_running)
{
    return std::thread(
        [poll_interval_ms, watched_files = &watched_files, source_labels = &source_labels, model_mutex = &model_mutex, model = &model, screen = &screen, keep_running = &keep_running]
        {
            while (*keep_running)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
                if (!*keep_running)
                {
                    break;
                }

                {
                    std::lock_guard lock(*model_mutex);
                    std::vector<slayerlog::WatcherLineBatch> watcher_batches;
                    watcher_batches.reserve(watched_files->size());

                    for (auto& watched_file : *watched_files)
                    {
                        slayerlog::WatcherLineBatch watcher_batch;
                        try
                        {
                            watched_file.watcher->poll(watcher_batch);
                        }
                        catch (const std::exception& ex)
                        {
                            // Ignore transient read errors while another process is writing.
                            SLAYERLOG_LOG_WARNING("Watcher poll threw for source=" << slayerlog::source_display_path(watched_file.source) << " error=" << ex.what());
                        }
                        catch (...)
                        {
                            // Ignore transient read errors while another process is writing.
                            SLAYERLOG_LOG_WARNING("Watcher poll threw for source=" << slayerlog::source_display_path(watched_file.source) << " error=<unknown>");
                        }

                        SLAYERLOG_LOG_TRACE("Live poll source=" << slayerlog::source_display_path(watched_file.source) << " returned_lines=" << watcher_batch.size());
                        watcher_batches.push_back(std::move(watcher_batch));
                    }

                    append_batch_to_model(watcher_batches, *source_labels, *model, *screen);
                }
            }
        });
}

std::optional<std::string> reload_tracked_sources(std::vector<slayerlog::LogSource> candidate_sources, std::vector<slayerlog::LogSource>& tracked_sources, std::vector<std::string>& source_labels, std::string& header_text,
                                                  std::vector<WatchedFile>& watched_files, slayerlog::LogModel& model, slayerlog::LogController& controller, ftxui::ScreenInteractive& screen)
{
    auto candidate_source_labels = slayerlog::build_source_labels(candidate_sources);
    std::string candidate_header = build_header_text(candidate_source_labels);
    std::vector<WatchedFile> candidate_watchers;
    std::vector<slayerlog::WatcherLineBatch> candidate_batches;

    try
    {
        candidate_watchers = create_file_watchers(candidate_sources, candidate_source_labels);
        candidate_batches  = collect_watcher_batches(candidate_watchers);
    }
    catch (const std::exception& ex)
    {
        return ex.what();
    }

    tracked_sources = std::move(candidate_sources);
    source_labels   = std::move(candidate_source_labels);
    header_text     = std::move(candidate_header);
    watched_files   = std::move(candidate_watchers);

    model.reset();
    controller.reset();
    model.set_show_source_labels(tracked_sources.size() > 1);
    append_batch_to_model(candidate_batches, source_labels, model, screen);

    return std::nullopt;
}

void register_commands(slayerlog::CommandManager& command_manager, slayerlog::LogModel& model, slayerlog::LogController& controller, std::function<int()> viewport_line_count,
                       std::function<slayerlog::CommandResult(std::string_view)> open_file_command, std::function<slayerlog::CommandResult()> close_open_file_command)
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
                                     [&, open_file_command](std::string_view arguments)
                                     {
                                         const std::string file_path = trim_text(arguments);
                                         if (file_path.empty())
                                         {
                                             return slayerlog::CommandResult {false, "Usage: open-file <path>"};
                                         }

                                         return open_file_command(file_path);
                                     });

    command_manager.register_command({"close-open-file", "Close one currently open file", "close-open-file"},
                                     [close_open_file_command](std::string_view arguments)
                                     {
                                         if (!trim_text(arguments).empty())
                                         {
                                             return slayerlog::CommandResult {false, "Usage: close-open-file"};
                                         }

                                         return close_open_file_command();
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
    slayerlog::debug_log::initialize(argc > 0 ? argv[0] : nullptr);
    SLAYERLOG_LOG_INFO("Debug log initialized at " << slayerlog::debug_log::log_file_path().string());

    const auto config                                 = slayerlog::parse_command_line(argc, argv);
    std::vector<slayerlog::LogSource> tracked_sources = parse_log_sources(config.file_paths);
    auto source_labels                                = slayerlog::build_source_labels(tracked_sources);
    std::string header_text                           = build_header_text(source_labels);
    SLAYERLOG_LOG_INFO("Starting slayerlog poll_interval_ms=" << config.poll_interval_ms << " watched_files=" << config.file_paths.size());
    for (std::size_t index = 0; index < tracked_sources.size(); ++index)
    {
        SLAYERLOG_LOG_INFO("Configured watcher[" << index << "] source=" << slayerlog::source_display_path(tracked_sources[index]) << " label=" << source_labels[index]);
    }

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    screen.TrackMouse();

    std::mutex model_mutex;
    slayerlog::LogModel model;
    model.set_show_source_labels(tracked_sources.size() > 1);

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
    auto watched_files = create_file_watchers(tracked_sources, source_labels);

    slayerlog::CommandPaletteController command_palette_controller(command_palette_model, command_manager, command_history);

    register_commands(
        command_manager, model, controller, [&] { return view.visible_line_count(screen.dimy()); },
        [&](std::string_view file_path)
        {
            slayerlog::LogSource candidate_source;
            try
            {
                candidate_source = slayerlog::parse_log_source(file_path);
            }
            catch (const std::exception& ex)
            {
                return slayerlog::CommandResult {false, ex.what()};
            }

            if (contains_tracked_source(tracked_sources, candidate_source))
            {
                return slayerlog::CommandResult {false, "File already open: " + std::string(file_path)};
            }

            std::vector<slayerlog::LogSource> candidate_sources = tracked_sources;
            candidate_sources.push_back(candidate_source);

            const auto error = reload_tracked_sources(candidate_sources, tracked_sources, source_labels, header_text, watched_files, model, controller, screen);
            if (error.has_value())
            {
                SLAYERLOG_LOG_ERROR("open-file failed file=" << file_path << " error=" << *error);
                return slayerlog::CommandResult {false, *error};
            }

            return slayerlog::CommandResult {true, "Opened file: " + std::string(file_path)};
        },
        [&]()
        {
            if (tracked_sources.empty())
            {
                return slayerlog::CommandResult {false, "No open files to close"};
            }

            command_palette_controller.open_close_open_file_picker(source_labels,
                                                                   [&](std::size_t selected_index)
                                                                   {
                                                                       if (selected_index >= tracked_sources.size())
                                                                       {
                                                                           return slayerlog::CommandResult {false, "Invalid open file selection"};
                                                                       }

                                                                       const std::string closed_label                      = source_labels[selected_index];
                                                                       std::vector<slayerlog::LogSource> candidate_sources = tracked_sources;
                                                                       candidate_sources.erase(candidate_sources.begin() + static_cast<std::ptrdiff_t>(selected_index));

                                                                       const auto error = reload_tracked_sources(candidate_sources, tracked_sources, source_labels, header_text, watched_files, model, controller, screen);
                                                                       if (error.has_value())
                                                                       {
                                                                           SLAYERLOG_LOG_ERROR("close-open-file failed selected_index=" << selected_index << " error=" << *error);
                                                                           return slayerlog::CommandResult {false, *error};
                                                                       }

                                                                       return slayerlog::CommandResult {true, "Closed file: " + closed_label};
                                                                   });

            return slayerlog::CommandResult {true, "Select a file to close", false};
        });

    slayerlog::MasterController master_controller(model, controller, view, screen, command_palette_controller);

    try
    {
        std::lock_guard lock(model_mutex);
        append_batch_to_model(collect_watcher_batches(watched_files), source_labels, model, screen);
    }
    catch (const std::exception& ex)
    {
        SLAYERLOG_LOG_ERROR("Initial watcher collection failed: " << ex.what());
        std::cerr << ex.what() << '\n';
        return 1;
    }

    std::atomic<bool> keep_running = true;
    std::thread watcher_thread     = start_watcher_thread(config.poll_interval_ms, watched_files, source_labels, model_mutex, model, screen, keep_running);

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
