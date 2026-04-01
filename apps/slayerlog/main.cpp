#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "command_line_parser.hpp"
#include "file_watcher.hpp"
#include "input_controller.hpp"
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

struct WatcherUpdateContext
{
    std::mutex& model_mutex;
    slayerlog::LogViewModel& model;
    ftxui::ScreenInteractive& screen;
    std::vector<slayerlog::ObservedLogUpdate>& initial_updates;
    std::atomic<bool>& collecting_initial;
    std::atomic<std::uint64_t>& current_poll_epoch;
    slayerlog::LogTimePoint open_time;
};

slayerlog::ObservedLogUpdate build_observed_update(
    const slayerlog::FileWatcher::Update& update,
    const std::string& source_path,
    const std::string& source_label,
    std::size_t source_index,
    WatcherUpdateContext& context)
{
    slayerlog::ObservedLogUpdate observed_update;
    observed_update.source_path  = source_path;
    observed_update.source_label = source_label;
    observed_update.kind         = update.kind;
    observed_update.lines        = update.lines;
    observed_update.source_index = source_index;
    observed_update.is_initial_load =
        context.collecting_initial.load() && update.kind == slayerlog::FileWatcher::Update::Kind::Snapshot;
    observed_update.poll_epoch = observed_update.is_initial_load ? 0 : context.current_poll_epoch.load();
    observed_update.observed_at = update.kind == slayerlog::FileWatcher::Update::Kind::Snapshot
                                      ? context.open_time
                                      : std::chrono::system_clock::now();
    return observed_update;
}

void handle_watcher_update(
    const slayerlog::FileWatcher::Update& update,
    const std::string& source_path,
    const std::string& source_label,
    std::size_t source_index,
    WatcherUpdateContext& context)
{
    auto observed_update = build_observed_update(update, source_path, source_label, source_index, context);
    if (observed_update.is_initial_load)
    {
        context.initial_updates.push_back(std::move(observed_update));
        return;
    }

    {
        std::lock_guard lock(context.model_mutex);
        context.model.apply_update(observed_update);
    }

    context.screen.PostEvent(ftxui::Event::Custom);
}

std::vector<std::unique_ptr<slayerlog::FileWatcher>> create_file_watchers(
    const std::vector<std::string>& file_paths,
    const std::vector<std::string>& source_labels,
    WatcherUpdateContext& context)
{
    std::vector<std::unique_ptr<slayerlog::FileWatcher>> watchers;
    watchers.reserve(file_paths.size());

    for (std::size_t index = 0; index < file_paths.size(); ++index)
    {
        const auto source_path  = file_paths[index];
        const auto source_label = source_labels[index];

        watchers.push_back(std::make_unique<slayerlog::FileWatcher>(
            source_path,
            [&, source_path, source_label, index](const slayerlog::FileWatcher::Update& update)
            {
                handle_watcher_update(update, source_path, source_label, index, context);
            }));
    }

    return watchers;
}

void apply_initial_snapshots(
    slayerlog::LogViewModel& model,
    std::mutex& model_mutex,
    std::vector<slayerlog::ObservedLogUpdate>& initial_updates,
    std::atomic<bool>& collecting_initial,
    ftxui::ScreenInteractive& screen)
{
    {
        std::lock_guard lock(model_mutex);
        model.apply_initial_updates(initial_updates);
    }

    collecting_initial = false;
    screen.PostEvent(ftxui::Event::Custom);
}

std::thread start_watcher_thread(
    int poll_interval_ms,
    std::vector<std::unique_ptr<slayerlog::FileWatcher>>& watchers,
    std::atomic<bool>& keep_running,
    std::atomic<std::uint64_t>& current_poll_epoch)
{
    return std::thread(
        [&]
        {
            while (keep_running)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
                if (!keep_running)
                {
                    break;
                }

                current_poll_epoch.fetch_add(1);
                for (auto& watcher : watchers)
                {
                    try
                    {
                        watcher->process_once();
                    }
                    catch (...)
                    {
                        // Ignore transient read errors while another process is writing.
                    }
                }
            }
        });
}

} // namespace

int main(int argc, char** argv)
{
    const auto config        = slayerlog::parse_command_line(argc, argv);
    const auto source_labels = build_source_labels(config.file_paths);
    const auto header_text   = build_header_text(source_labels);

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    screen.TrackMouse();

    std::mutex model_mutex;
    slayerlog::LogViewModel model;
    model.set_show_source_labels(config.file_paths.size() > 1);
    slayerlog::LogView view;
    slayerlog::InputController input_controller(model, view, screen);
    // FileWatcher emits its initial snapshot from the constructor, so collect
    // those callbacks first and apply them as one startup batch.
    std::vector<slayerlog::ObservedLogUpdate> initial_updates;
    initial_updates.reserve(config.file_paths.size());
    std::atomic<bool> collecting_initial = true;
    std::atomic<std::uint64_t> current_poll_epoch = 0;
    WatcherUpdateContext watcher_context {
        model_mutex,
        model,
        screen,
        initial_updates,
        collecting_initial,
        current_poll_epoch,
        std::chrono::system_clock::now(),
    };

    std::vector<std::unique_ptr<slayerlog::FileWatcher>> watchers;
    try
    {
        watchers = create_file_watchers(config.file_paths, source_labels, watcher_context);
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n';
        return 1;
    }

    apply_initial_snapshots(model, model_mutex, initial_updates, collecting_initial, screen);

    std::atomic<bool> keep_running = true;
    std::thread watcher_thread =
        start_watcher_thread(config.poll_interval_ms, watchers, keep_running, current_poll_epoch);

    auto viewer = ftxui::Renderer(
        [&]
        {
            std::lock_guard lock(model_mutex);
            return view.render(model, header_text);
        });

    viewer |= ftxui::CatchEvent(
        [&](ftxui::Event event)
        {
            std::lock_guard lock(model_mutex);
            return input_controller.handle_event(event);
        });

    screen.Loop(viewer);
    keep_running = false;
    if (watcher_thread.joinable())
    {
        watcher_thread.join();
    }

    return 0;
}
