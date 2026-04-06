#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "command_line_parser.hpp"
#include "command_history.hpp"
#include "command_palette_controller.hpp"
#include "command_palette_model.hpp"
#include "command_manager.hpp"
#include "debug_log.hpp"
#include "file_watcher.hpp"
#include "input_controller.hpp"
#include "log_batch.hpp"
#include "log_controller.hpp"
#include "log_view.hpp"
#include "log_model.hpp"
#include "settings_store.hpp"

namespace
{

/**
 * @brief Builds the display label for each opened source file.
 *
 * Uses the basename when it is unique across all opened files, otherwise falls
 * back to the full path so duplicate filenames remain distinguishable.
 */
std::vector<std::string> build_source_labels(const std::vector<std::string>& file_paths)
{
    std::unordered_map<std::string, int> basename_counts;
    for (const auto& file_path : file_paths)
    {
        const auto basename = std::filesystem::path(file_path).filename().string();
        ++basename_counts[basename];
    }

    std::vector<std::string> labels;
    labels.reserve(file_paths.size());
    for (const auto& file_path : file_paths)
    {
        const auto basename = std::filesystem::path(file_path).filename().string();
        if (!basename.empty() && basename_counts[basename] == 1)
        {
            labels.push_back(basename);
        }
        else
        {
            labels.push_back(file_path);
        }
    }

    return labels;
}

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

std::string normalize_file_path_for_comparison(std::string_view file_path)
{
    const std::filesystem::path input_path(file_path);
    std::error_code error_code;
    std::filesystem::path normalized_path = std::filesystem::weakly_canonical(input_path, error_code);
    if (error_code)
    {
        error_code.clear();
        normalized_path = std::filesystem::absolute(input_path, error_code);
        if (error_code)
        {
            normalized_path = input_path.lexically_normal();
        }
        else
        {
            normalized_path = normalized_path.lexically_normal();
        }
    }

    std::string normalized_text = normalized_path.make_preferred().string();
#ifdef _WIN32
    std::transform(normalized_text.begin(), normalized_text.end(), normalized_text.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
#endif

    return normalized_text;
}

bool contains_tracked_file_path(const std::vector<std::string>& tracked_file_paths, std::string_view candidate_file_path)
{
    const std::string normalized_candidate_path = normalize_file_path_for_comparison(candidate_file_path);

    return std::any_of(tracked_file_paths.begin(), tracked_file_paths.end(), [&](const std::string& tracked_file_path)
                       { return normalize_file_path_for_comparison(tracked_file_path) == normalized_candidate_path; });
}

struct WatchedFile
{
    std::string file_path;
    std::string source_label;
    std::unique_ptr<slayerlog::FileWatcher> watcher;
};

std::vector<WatchedFile> create_file_watchers(const std::vector<std::string>& file_paths, const std::vector<std::string>& source_labels)
{
    std::vector<WatchedFile> watched_files;
    watched_files.reserve(file_paths.size());

    for (std::size_t index = 0; index < file_paths.size(); ++index)
    {
        watched_files.push_back(WatchedFile {
            file_paths[index],
            source_labels[index],
            std::make_unique<slayerlog::FileWatcher>(file_paths[index]),
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
        SLAYERLOG_LOG_TRACE("Initial poll file=" << watched_file.file_path << " returned_lines=" << watcher_batch.size());
        watcher_batches.push_back(std::move(watcher_batch));
    }

    return watcher_batches;
}

void append_batch_to_model(const std::vector<slayerlog::WatcherLineBatch>& watcher_batches, const std::vector<std::string>& source_labels,
                           slayerlog::LogModel& model, ftxui::ScreenInteractive& screen)
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

std::thread start_watcher_thread(int poll_interval_ms, std::vector<WatchedFile>& watched_files,
                                 const std::vector<std::string>& source_labels, std::mutex& model_mutex, slayerlog::LogModel& model,
                                 ftxui::ScreenInteractive& screen, std::atomic<bool>& keep_running)
{
    return std::thread(
        [poll_interval_ms, watched_files = &watched_files, source_labels = &source_labels, model_mutex = &model_mutex, model = &model,
         screen = &screen, keep_running = &keep_running]
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
                            SLAYERLOG_LOG_WARNING("Watcher poll threw for file=" << watched_file.file_path << " error=" << ex.what());
                        }
                        catch (...)
                        {
                            // Ignore transient read errors while another process is writing.
                            SLAYERLOG_LOG_WARNING("Watcher poll threw for file=" << watched_file.file_path << " error=<unknown>");
                        }

                        SLAYERLOG_LOG_TRACE("Live poll file=" << watched_file.file_path << " returned_lines=" << watcher_batch.size());
                        watcher_batches.push_back(std::move(watcher_batch));
                    }

                    append_batch_to_model(watcher_batches, *source_labels, *model, *screen);
                }
            }
        });
}

void register_commands(slayerlog::CommandManager& command_manager, slayerlog::LogModel& model, slayerlog::LogController& controller,
                       std::function<int()> viewport_line_count,
                       std::function<slayerlog::CommandResult(std::string_view)> open_file_command)
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
                                             return slayerlog::CommandResult {false,
                                                                              "Invalid filter-in pattern: " + std::string(error.what())};
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
                                             return slayerlog::CommandResult {false,
                                                                              "Invalid filter-out pattern: " + std::string(error.what())};
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
                                             return slayerlog::CommandResult {false,
                                                                              "Line " + std::to_string(*line_number) + " is out of range"};
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
                                                 "Find active: " + model.find_query() + " (" + std::to_string(visible_matches) +
                                                     " visible / " + std::to_string(total_matches) + " total)",
                                             };
                                         }

                                         return slayerlog::CommandResult {
                                             true,
                                             "Find active: " + model.find_query() + " (0 visible / " + std::to_string(total_matches) +
                                                 " total)",
                                         };
                                     });
}

} // namespace

int main(int argc, char** argv)
{
    slayerlog::debug_log::initialize();
    SLAYERLOG_LOG_INFO("Debug log initialized at " << slayerlog::debug_log::log_file_path().string());

    const auto config                           = slayerlog::parse_command_line(argc, argv);
    std::vector<std::string> tracked_file_paths = config.file_paths;
    auto source_labels                          = build_source_labels(tracked_file_paths);
    std::string header_text                     = build_header_text(source_labels);
    SLAYERLOG_LOG_INFO("Starting slayerlog poll_interval_ms=" << config.poll_interval_ms << " watched_files=" << config.file_paths.size());
    for (std::size_t index = 0; index < tracked_file_paths.size(); ++index)
    {
        SLAYERLOG_LOG_INFO("Configured watcher[" << index << "] file=" << tracked_file_paths[index] << " label=" << source_labels[index]);
    }

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    screen.TrackMouse();

    std::mutex model_mutex;
    slayerlog::LogModel model;
    model.set_show_source_labels(tracked_file_paths.size() > 1);

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
    slayerlog::LogController controller;
    auto watched_files = create_file_watchers(tracked_file_paths, source_labels);

    register_commands(
        command_manager, model, controller, [&] { return view.visible_line_count(screen.dimy()); },
        [&](std::string_view file_path)
        {
            if (contains_tracked_file_path(tracked_file_paths, file_path))
            {
                return slayerlog::CommandResult {false, "File already open: " + std::string(file_path)};
            }

            std::vector<std::string> candidate_file_paths = tracked_file_paths;
            candidate_file_paths.push_back(std::string(file_path));

            auto candidate_source_labels = build_source_labels(candidate_file_paths);
            std::string candidate_header = build_header_text(candidate_source_labels);
            std::vector<WatchedFile> candidate_watchers;
            std::vector<slayerlog::WatcherLineBatch> candidate_batches;

            try
            {
                candidate_watchers = create_file_watchers(candidate_file_paths, candidate_source_labels);
                candidate_batches  = collect_watcher_batches(candidate_watchers);
            }
            catch (const std::exception& ex)
            {
                SLAYERLOG_LOG_ERROR("open-file failed file=" << file_path << " error=" << ex.what());
                return slayerlog::CommandResult {false, ex.what()};
            }

            tracked_file_paths = std::move(candidate_file_paths);
            source_labels      = std::move(candidate_source_labels);
            header_text        = std::move(candidate_header);
            watched_files      = std::move(candidate_watchers);

            model.reset();
            controller.reset();
            model.set_show_source_labels(tracked_file_paths.size() > 1);
            append_batch_to_model(candidate_batches, source_labels, model, screen);

            return slayerlog::CommandResult {true, "Opened file: " + std::string(file_path)};
        });

    slayerlog::CommandPaletteController command_palette_controller(command_palette_model, command_manager, command_history);
    slayerlog::InputController input_controller(model, controller, view, screen, command_palette_controller);

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
    std::thread watcher_thread =
        start_watcher_thread(config.poll_interval_ms, watched_files, source_labels, model_mutex, model, screen, keep_running);

    auto viewer = ftxui::Renderer(
        [&]
        {
            std::lock_guard lock(model_mutex);
            return view.render(model, controller, header_text, screen.dimy(), input_controller.command_palette());
        });

    viewer |= ftxui::CatchEvent(
        [&](ftxui::Event event)
        {
            std::lock_guard lock(model_mutex);
            return input_controller.handle_event(event);
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
