#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "command_manager.hpp"

namespace slayerlog
{

enum class CommandPaletteMode
{
    Commands,
    History,
    CloseOpenFile,
};

struct CommandPaletteModel
{
    bool open               = false;
    CommandPaletteMode mode = CommandPaletteMode::Commands;
    std::string query;
    std::size_t cursor_position = 0;
    std::vector<CommandDescriptor> matching_commands;
    std::vector<std::string> matching_history_entries;
    std::vector<std::string> open_files;
    int selected_index = 0;
    std::string status_message;
    bool status_is_error = false;
};

} // namespace slayerlog
