#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "command_line_parser.hpp"
#include "command_palette_controller.hpp"
#include "command_palette_model.hpp"
#include "command_palette_view.hpp"
#include "command_registrar.hpp"
#include "command_manager.hpp"
#include "commands/command_history.hpp"
#include "debug_log.hpp"
#include "log_controller.hpp"
#include "log_model.hpp"
#include "log_timestamp.hpp"
#include "log_view.hpp"
#include "master_controller.hpp"
#include "master_view.hpp"
#include "settings_store.hpp"
#include "tracked_source_manager.hpp"

namespace
{

constexpr std::string_view timestamp_formats_section = "timestamp_formats";
constexpr std::string_view timestamp_format_key      = "format";

void append_batch_to_model(const slayerlog::LogBatch& batch, slayerlog::LogModel& model, slayerlog::LogController& controller, ftxui::ScreenInteractive& screen)
{
    if (batch.empty())
    {
        return;
    }

    model.append_batch(batch);
    controller.sync_view(model);
    screen.PostEvent(ftxui::Event::Custom);
}

std::thread start_watcher_thread(int poll_interval_ms, slayerlog::TrackedSourceManager& tracked_source_manager, std::mutex& model_mutex, slayerlog::LogModel& model, slayerlog::LogController& controller, ftxui::ScreenInteractive& screen,
                                 std::atomic<bool>& keep_running)
{
    return std::thread(
        [poll_interval_ms, tracked_source_manager = &tracked_source_manager, model_mutex = &model_mutex, model = &model, controller = &controller, screen = &screen, keep_running = &keep_running]
        {
            while (*keep_running)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
                if (!*keep_running)
                {
                    break;
                }

                std::lock_guard lock(*model_mutex);
                append_batch_to_model(tracked_source_manager->poll(), *model, *controller, *screen);
            }
        });
}

} // namespace

int main(int argc, char** argv)
{
    slayerlog::debug_log::initialize(argc > 0 ? argv[0] : nullptr);
    SLAYERLOG_LOG_INFO("Debug log initialized at " << slayerlog::debug_log::log_file_path().string());

    const auto config       = slayerlog::parse_command_line(argc, argv);
    std::string header_text = slayerlog::build_header_text({});

    slayerlog::SettingsStore settings_store(slayerlog::default_settings_file_path());
    std::string settings_error_message;
    if (!settings_store.load(settings_error_message))
    {
        SLAYERLOG_LOG_WARNING("Failed to load settings from " << settings_store.file_path() << ": " << settings_error_message);
        settings_error_message.clear();
    }

    if (!settings_store.ensure_default_values(timestamp_formats_section, timestamp_format_key, slayerlog::default_timestamp_formats(), settings_error_message))
    {
        SLAYERLOG_LOG_WARNING("Failed to seed timestamp formats in settings file " << settings_store.file_path() << ": " << settings_error_message);
        settings_error_message.clear();
    }

    auto timestamp_catalog = std::make_shared<const slayerlog::TimestampFormatCatalog>(settings_store.ini().values(timestamp_formats_section, timestamp_format_key));
    slayerlog::set_default_timestamp_format_catalog(timestamp_catalog);
    slayerlog::TrackedSourceManager tracked_source_manager(timestamp_catalog);

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

    slayerlog::CommandHistory command_history(settings_store);
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

    slayerlog::register_commands(command_manager, model, controller, command_palette_controller, header_text, screen, tracked_source_manager);

    slayerlog::MasterController master_controller(model, controller, view, screen, command_palette_controller);

    {
        std::lock_guard lock(model_mutex);
        slayerlog::reload_model_from_manager(tracked_source_manager, header_text, model, controller, screen);
    }

    std::atomic<bool> keep_running = true;
    std::thread watcher_thread     = start_watcher_thread(config.poll_interval_ms, tracked_source_manager, model_mutex, model, controller, screen, keep_running);

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
