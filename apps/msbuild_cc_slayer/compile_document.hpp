#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace vscc
{
namespace fs = std::filesystem;

struct CompilerOptions
{
    std::string compiler;
    std::string compile_as;
    std::optional<std::string> language_standard;
    std::vector<fs::path> include_directories;
    std::vector<std::string> definitions;
    std::vector<std::string> undefinitions;
    std::vector<fs::path> forced_include_files;
    std::vector<std::string> additional_options;
    std::optional<std::string> warning_level;
    bool treat_warning_as_error = false;
    std::optional<std::string> runtime_library;
    std::optional<std::string> optimization;
    std::optional<std::string> debug_information_format;
    std::optional<std::string> exception_handling;
    std::optional<std::string> runtime_type_info;
    std::vector<std::string> disabled_warnings;
    std::optional<std::string> precompiled_header_mode;
    std::optional<std::string> precompiled_header_file;
};

struct CompileFile
{
    fs::path directory;
    fs::path file;
    std::optional<fs::path> output;
    CompilerOptions options;
};

struct CompilationDocument
{
    std::vector<CompileFile> files;
};
} // namespace vscc
