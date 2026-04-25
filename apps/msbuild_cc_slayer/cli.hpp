#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace vscc
{
namespace fs = std::filesystem;

struct CliOptions
{
    fs::path input_path;
    std::optional<std::string> configuration;
    std::optional<std::string> platform;
    fs::path output_path;
    std::optional<fs::path> msbuild_path;
    unsigned jobs = 1;
    bool verbose = false;
    bool debug = false;
    bool dry_run = false;
    bool strict = false;
    bool help = false;
};

CliOptions parse_cli(int argc, char** argv);
void print_help();
} // namespace vscc
