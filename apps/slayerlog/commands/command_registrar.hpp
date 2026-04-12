#pragma once

#include <functional>
#include <string>
#include <vector>

#include <ftxui/component/screen_interactive.hpp>

namespace slayerlog
{

class CommandManager;
class CommandPaletteController;
class LogController;
class LogModel;
class TrackedSourceManager;

std::string build_header_text(const std::vector<std::string>& labels);
void reload_model_from_manager(const TrackedSourceManager& tracked_source_manager, std::string& header_text, LogModel& model, ftxui::ScreenInteractive& screen);
void register_commands(CommandManager& command_manager, LogModel& model, LogController& controller, std::function<int()> viewport_line_count, CommandPaletteController& command_palette_controller, std::string& header_text,
                       ftxui::ScreenInteractive& screen, TrackedSourceManager& tracked_source_manager);

} // namespace slayerlog
