#pragma once

#include <string>

namespace slayerlog
{

struct Config
{
    std::string file_path;
    int poll_interval_ms = 250;
};

Config parse_command_line(int argc, char* argv[]);

} // namespace slayerlog
