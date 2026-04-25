#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace vscc
{
namespace fs = std::filesystem;

struct ConfigurationPair
{
    std::string configuration;
    std::string platform;
};

struct ProcessResult
{
    int exit_code = 0;
    std::string output;
};

std::string trim(const std::string& text);
std::string to_lower(std::string text);
bool iequals(const std::string& lhs, const std::string& rhs);
std::vector<std::string> split(const std::string& text, char delimiter);
std::optional<ConfigurationPair> parse_pair(const std::string& text);
std::string pair_to_string(const ConfigurationPair& pair);
std::string path_string(const fs::path& path);
fs::path absolute_normal(const fs::path& path);
fs::path resolve_against(const fs::path& base, const std::string& value);
bool is_truthy_msbuild(const std::string& value);
std::string json_escape(const std::string& value);
std::string quote_command_arg(const std::string& arg);
std::string command_line_for_debug(const std::vector<std::string>& args);
ProcessResult run_process(const std::vector<std::string>& args);
} // namespace vscc
