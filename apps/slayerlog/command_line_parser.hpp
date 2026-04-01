#pragma once

#include <string>
#include <vector>

namespace slayerlog
{

struct Config
{
    std::vector<std::string> file_paths;
    int poll_interval_ms = 250;
};

Config parse_command_line(int argc, char* argv[]);

} // namespace slayerlog
