#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "command_manager.hpp"

namespace slayerlog
{

struct CommandPaletteModel
{
    bool open = false;
    std::string query;
    std::size_t cursor_position = 0;
    std::vector<CommandDescriptor> matching_commands;
    int selected_index = 0;
    std::string status_message;
    bool status_is_error = false;
};

} // namespace slayerlog
