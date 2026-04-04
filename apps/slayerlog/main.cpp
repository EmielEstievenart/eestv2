#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "command_line_parser.hpp"
#include "command_palette_controller.hpp"
#include "command_palette_model.hpp"
#include "command_manager.hpp"
#include "debug_log.hpp"
#include "file_watcher.hpp"
#include "input_controller.hpp"
#include "log_batch.hpp"
#include "log_view.hpp"
#include "log_view_model.hpp"

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
                           std::mutex& model_mutex, slayerlog::LogViewModel& model, ftxui::ScreenInteractive& screen)
{
    const auto merged_lines = slayerlog::merge_log_batch(watcher_batches, source_labels);
    SLAYERLOG_LOG_TRACE("Merging watcher batches watcher_count=" << watcher_batches.size() << " merged_lines=" << merged_lines.size());
    if (merged_lines.empty())
    {
        return;
    }

    {
        std::lock_guard lock(model_mutex);
        model.append_lines(merged_lines);
    }

    screen.PostEvent(ftxui::Event::Custom);
}

std::thread start_watcher_thread(int poll_interval_ms, std::vector<WatchedFile>& watched_files,
                                 const std::vector<std::string>& source_labels, std::mutex& model_mutex, slayerlog::LogViewModel& model,
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

                append_batch_to_model(watcher_batches, *source_labels, *model_mutex, *model, *screen);
            }
        });
}

void register_commands(slayerlog::CommandManager& command_manager, slayerlog::LogViewModel& model)
{
    command_manager.register_command({"filter-in", "Show lines containing text", "filter-in <text>"},
                                     [&](std::string_view arguments)
                                     {
                                         if (arguments.empty())
                                         {
                                             return slayerlog::CommandResult {false, "Usage: filter-in <text>"};
                                         }

                                         model.add_include_filter(std::string(arguments));
                                         return slayerlog::CommandResult {true, "Added include filter: " + std::string(arguments)};
                                     });

    command_manager.register_command({"filter-out", "Hide lines containing text", "filter-out <text>"},
                                     [&](std::string_view arguments)
                                     {
                                         if (arguments.empty())
                                         {
                                             return slayerlog::CommandResult {false, "Usage: filter-out <text>"};
                                         }

                                         model.add_exclude_filter(std::string(arguments));
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

    command_manager.register_command({"go-to-line", "Center the view on a line number", "go-to-line <line-number>"},
                                     [&](std::string_view arguments)
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

                                         if (!model.center_on_line_number(*line_number))
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
}

} // namespace

int main(int argc, char** argv)
{
    slayerlog::debug_log::initialize();
    SLAYERLOG_LOG_INFO("Debug log initialized at " << slayerlog::debug_log::log_file_path().string());

    const auto config        = slayerlog::parse_command_line(argc, argv);
    const auto source_labels = build_source_labels(config.file_paths);
    const auto header_text   = build_header_text(source_labels);
    SLAYERLOG_LOG_INFO("Starting slayerlog poll_interval_ms=" << config.poll_interval_ms << " watched_files=" << config.file_paths.size());
    for (std::size_t index = 0; index < config.file_paths.size(); ++index)
    {
        SLAYERLOG_LOG_INFO("Configured watcher[" << index << "] file=" << config.file_paths[index] << " label=" << source_labels[index]);
    }

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    screen.TrackMouse();

    std::mutex model_mutex;
    slayerlog::LogViewModel model;
    model.set_show_source_labels(config.file_paths.size() > 1);
    slayerlog::CommandPaletteModel command_palette_model;
    slayerlog::CommandManager command_manager;
    register_commands(command_manager, model);
    slayerlog::CommandPaletteController command_palette_controller(command_palette_model, command_manager);
    slayerlog::LogView view;
    slayerlog::InputController input_controller(model, view, screen, command_palette_controller);

    auto watched_files = create_file_watchers(config.file_paths, source_labels);
    try
    {
        append_batch_to_model(collect_watcher_batches(watched_files), source_labels, model_mutex, model, screen);
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
            return view.render(model, header_text, screen.dimy(), input_controller.command_palette());
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
