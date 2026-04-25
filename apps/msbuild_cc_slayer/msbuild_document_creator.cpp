#include "msbuild_document_creator.hpp"

#include "json_parser.hpp"
#include "utility.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <future>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace vscc
{
namespace
{
struct SolutionProject
{
    std::string name;
    fs::path path;
    std::string extension;
    std::string project_guid;
};

struct ProjectConfigurationMapping
{
    std::optional<ConfigurationPair> active_cfg;
    bool build = false;
};

struct SolutionInfo
{
    fs::path path;
    fs::path directory;
    std::vector<ConfigurationPair> configurations;
    std::vector<SolutionProject> projects;
    std::map<std::string, std::map<std::string, ProjectConfigurationMapping>> project_configurations;
};

struct ProjectSelection
{
    fs::path project_path;
    std::string project_name;
    ConfigurationPair solution_pair;
    ConfigurationPair project_pair;
};

struct ClCompileItem
{
    std::string identity;
    fs::path full_path;
    std::map<std::string, std::string> metadata;
};

struct EvaluatedProject
{
    fs::path project_path;
    fs::path project_directory;
    ConfigurationPair project_pair;
    std::map<std::string, std::string> properties;
    std::vector<ClCompileItem> cl_compile_items;
};

std::string value_or_empty(const std::map<std::string, std::string>& values, const std::string& key)
{
    const auto it = values.find(key);
    return it == values.end() ? std::string{} : it->second;
}

std::string metadata_or_empty(const ClCompileItem& item, const std::string& key)
{
    return value_or_empty(item.metadata, key);
}

bool platform_matches(const std::string& requested, const std::string& actual)
{
    if (iequals(requested, actual))
    {
        return true;
    }
    return iequals(requested, "x86") && iequals(actual, "Win32");
}

bool pair_matches(const ConfigurationPair& pair, const CliOptions& options)
{
    if (options.configuration && !iequals(*options.configuration, pair.configuration))
    {
        return false;
    }
    if (options.platform && !platform_matches(*options.platform, pair.platform))
    {
        return false;
    }
    return true;
}

std::optional<ConfigurationPair> select_configuration_pair(const std::vector<ConfigurationPair>& pairs, const CliOptions& options)
{
    const auto it = std::find_if(pairs.begin(), pairs.end(), [&](const ConfigurationPair& pair) { return pair_matches(pair, options); });
    if (it != pairs.end())
    {
        return *it;
    }
    if (!options.configuration && !options.platform && !pairs.empty())
    {
        return pairs.front();
    }
    return std::nullopt;
}

std::string available_pairs_message(const std::vector<ConfigurationPair>& pairs)
{
    std::ostringstream stream;
    stream << "Available configurations:\n";
    for (const ConfigurationPair& pair : pairs)
    {
        stream << "  " << pair_to_string(pair) << '\n';
    }
    return stream.str();
}

std::optional<fs::path> find_on_path(const std::string& executable)
{
    const char* path_env = std::getenv("PATH");
    if (path_env == nullptr)
    {
        return std::nullopt;
    }

#if defined(_WIN32)
    constexpr char path_separator = ';';
#else
    constexpr char path_separator = ':';
#endif

    for (const std::string& entry : split(path_env, path_separator))
    {
        if (entry.empty())
        {
            continue;
        }
        fs::path candidate = fs::path(entry) / executable;
        std::error_code ec;
        if (fs::exists(candidate, ec) && !fs::is_directory(candidate, ec))
        {
            return absolute_normal(candidate);
        }
    }
    return std::nullopt;
}

std::optional<fs::path> locate_with_vswhere()
{
#if defined(_WIN32)
    const char* program_files_x86 = std::getenv("ProgramFiles(x86)");
    if (program_files_x86 == nullptr)
    {
        return std::nullopt;
    }
    const fs::path vswhere = fs::path(program_files_x86) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe";
    std::error_code ec;
    if (!fs::exists(vswhere, ec))
    {
        return std::nullopt;
    }

    const ProcessResult result = run_process({path_string(vswhere), "-latest", "-products", "*", "-requires", "Microsoft.Component.MSBuild", "-find", "MSBuild\\**\\Bin\\MSBuild.exe"});
    if (result.exit_code != 0)
    {
        return std::nullopt;
    }
    for (const std::string& line : split(result.output, '\n'))
    {
        const fs::path candidate(trim(line));
        if (!candidate.empty() && fs::exists(candidate, ec))
        {
            return absolute_normal(candidate);
        }
    }
#endif
    return std::nullopt;
}

fs::path locate_msbuild(const CliOptions& options)
{
    if (options.msbuild_path)
    {
        std::error_code ec;
        if (!fs::exists(*options.msbuild_path, ec))
        {
            throw std::runtime_error("MSBuild path does not exist: " + path_string(*options.msbuild_path));
        }
        return absolute_normal(*options.msbuild_path);
    }

    const char* env_path = std::getenv("MSBUILD_EXE_PATH");
    if (env_path != nullptr && *env_path != '\0')
    {
        fs::path path(env_path);
        std::error_code ec;
        if (fs::exists(path, ec))
        {
            return absolute_normal(path);
        }
    }
    if (auto path = find_on_path("MSBuild.exe"))
    {
        return *path;
    }
    if (auto path = find_on_path("msbuild"))
    {
        return *path;
    }
    if (auto path = locate_with_vswhere())
    {
        return *path;
    }
    throw std::runtime_error("MSBuild.exe was not found. Install Visual Studio Build Tools or pass --msbuild-path.");
}

std::optional<std::pair<int, int>> parse_version(const std::string& output)
{
    for (const std::string& token : split(output, '\n'))
    {
        const std::string line = trim(token);
        if (line.empty() || !std::isdigit(static_cast<unsigned char>(line.front())))
        {
            continue;
        }
        const std::vector<std::string> parts = split(line, '.');
        if (parts.size() >= 2)
        {
            return std::pair<int, int>{std::stoi(parts[0]), std::stoi(parts[1])};
        }
    }
    return std::nullopt;
}

void validate_msbuild_version(const fs::path& msbuild, const CliOptions& options, Diagnostics& diagnostics)
{
    const ProcessResult result = run_process({path_string(msbuild), "-nologo", "-version"});
    if (result.exit_code != 0)
    {
        throw std::runtime_error("failed to run MSBuild version check: " + trim(result.output));
    }
    const auto version = parse_version(result.output);
    if (!version)
    {
        throw std::runtime_error("failed to parse MSBuild version output: " + trim(result.output));
    }
    if (version->first < 17 || (version->first == 17 && version->second < 8))
    {
        throw std::runtime_error("MSBuild 17.8 or newer is required for -getItem/-getProperty support.");
    }
    diagnostics.verbose(options, "using MSBuild: " + path_string(msbuild) + " version " + std::to_string(version->first) + "." + std::to_string(version->second));
}

std::vector<ConfigurationPair> read_project_configuration_items(const JsonValue& root)
{
    std::vector<ConfigurationPair> pairs;
    const auto* root_object = root.object();
    const auto* items_value = root_object == nullptr ? nullptr : object_member(*root_object, "Items");
    const auto* items_object = items_value == nullptr ? nullptr : items_value->object();
    const auto* configs_value = items_object == nullptr ? nullptr : object_member(*items_object, "ProjectConfiguration");
    const auto* configs = configs_value == nullptr ? nullptr : configs_value->array();
    if (configs == nullptr)
    {
        return pairs;
    }

    for (const JsonValue& item : *configs)
    {
        const auto* item_object = item.object();
        const auto* identity = item_object == nullptr ? nullptr : object_member(*item_object, "Identity");
        if (identity != nullptr && identity->string() != nullptr)
        {
            if (auto pair = parse_pair(*identity->string()))
            {
                pairs.push_back(*pair);
            }
        }
    }
    return pairs;
}

std::vector<ConfigurationPair> read_project_configurations_from_xml(const fs::path& project_path)
{
    std::vector<ConfigurationPair> pairs;
    std::ifstream project_file(project_path);
    std::string line;
    while (std::getline(project_file, line))
    {
        const std::string marker = "<ProjectConfiguration Include=\"";
        const std::size_t begin = line.find(marker);
        if (begin == std::string::npos)
        {
            continue;
        }
        const std::size_t value_begin = begin + marker.size();
        const std::size_t value_end = line.find('"', value_begin);
        if (value_end == std::string::npos)
        {
            continue;
        }
        if (auto pair = parse_pair(line.substr(value_begin, value_end - value_begin)))
        {
            pairs.push_back(*pair);
        }
    }
    return pairs;
}

std::vector<ConfigurationPair> query_project_configurations(const fs::path& msbuild, const fs::path& project_path, const CliOptions& options, Diagnostics& diagnostics)
{
    const std::vector<std::string> args{path_string(msbuild), path_string(project_path), "-nologo", "-verbosity:quiet", "-tl:off", "-nr:false", "-getItem:ProjectConfiguration"};
    diagnostics.debug(options, "invoking: " + command_line_for_debug(args));

    const ProcessResult result = run_process(args);
    diagnostics.debug(options, "MSBuild exit code: " + std::to_string(result.exit_code));
    if (result.exit_code == 0)
    {
        return read_project_configuration_items(parse_json_output(result.output));
    }

    std::vector<ConfigurationPair> fallback_pairs = read_project_configurations_from_xml(project_path);
    if (!fallback_pairs.empty())
    {
        diagnostics.debug(options, "falling back to ProjectConfiguration Include parsing for " + path_string(project_path));
        return fallback_pairs;
    }

    throw std::runtime_error("failed to query project configurations for " + path_string(project_path) + ": " + trim(result.output));
}

std::vector<std::string> msbuild_property_query()
{
    return {"MSBuildProjectDirectory",      "ProjectDir",       "IntDir",                  "OutDir",       "TargetName",
            "TargetExt",                    "PlatformToolset", "VCToolsInstallDir",       "VCToolsVersion",
            "WindowsTargetPlatformVersion", "IncludePath",     "ExternalIncludePath",     "CLToolExe",    "CLToolPath"};
}

std::string comma_join(const std::vector<std::string>& values)
{
    std::ostringstream stream;
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        if (i != 0)
        {
            stream << ',';
        }
        stream << values[i];
    }
    return stream.str();
}

std::map<std::string, std::string> read_properties(const JsonValue& root)
{
    std::map<std::string, std::string> properties;
    const auto* root_object = root.object();
    const auto* properties_value = root_object == nullptr ? nullptr : object_member(*root_object, "Properties");
    const auto* properties_object = properties_value == nullptr ? nullptr : properties_value->object();
    if (properties_object == nullptr)
    {
        return properties;
    }

    for (const auto& [key, value] : *properties_object)
    {
        properties.emplace(key, json_value_to_string(value));
    }
    return properties;
}

std::vector<ClCompileItem> read_cl_compile_items(const JsonValue& root)
{
    std::vector<ClCompileItem> items;
    const auto* root_object = root.object();
    const auto* items_value = root_object == nullptr ? nullptr : object_member(*root_object, "Items");
    const auto* items_object = items_value == nullptr ? nullptr : items_value->object();
    const auto* cl_compile_value = items_object == nullptr ? nullptr : object_member(*items_object, "ClCompile");
    const auto* cl_compile = cl_compile_value == nullptr ? nullptr : cl_compile_value->array();
    if (cl_compile == nullptr)
    {
        return items;
    }

    for (const JsonValue& item_value : *cl_compile)
    {
        const auto* item_object = item_value.object();
        if (item_object == nullptr)
        {
            continue;
        }

        ClCompileItem item;
        for (const auto& [key, value] : *item_object)
        {
            const std::string text = json_value_to_string(value);
            item.metadata.emplace(key, text);
            if (key == "Identity")
            {
                item.identity = text;
            }
            else if (key == "FullPath")
            {
                item.full_path = fs::path(text);
            }
        }
        items.push_back(std::move(item));
    }
    return items;
}

EvaluatedProject evaluate_project(const fs::path& msbuild, const ProjectSelection& selection, const CliOptions& options, Diagnostics& diagnostics)
{
    const std::vector<std::string> args{path_string(msbuild),
                                        path_string(selection.project_path),
                                        "-nologo",
                                        "-verbosity:quiet",
                                        "-tl:off",
                                        "-nr:false",
                                        "-p:Configuration=" + selection.project_pair.configuration,
                                        "-p:Platform=" + selection.project_pair.platform,
                                        "-getProperty:" + comma_join(msbuild_property_query()),
                                        "-getItem:ClCompile"};
    diagnostics.debug(options, "invoking: " + command_line_for_debug(args));
    const ProcessResult result = run_process(args);
    diagnostics.debug(options, "MSBuild exit code: " + std::to_string(result.exit_code));
    if (result.exit_code != 0)
    {
        throw std::runtime_error("MSBuild exited with code " + std::to_string(result.exit_code) + ": " + trim(result.output));
    }

    JsonValue root = parse_json_output(result.output);
    EvaluatedProject project;
    project.project_path = selection.project_path;
    project.project_directory = absolute_normal(selection.project_path.parent_path());
    project.project_pair = selection.project_pair;
    project.properties = read_properties(root);
    project.cl_compile_items = read_cl_compile_items(root);
    diagnostics.verbose(options, "evaluated: " + path_string(selection.project_path.filename()) + " " + pair_to_string(selection.project_pair) + " -> " +
                                     std::to_string(project.cl_compile_items.size()) + " ClCompile items");
    return project;
}

SolutionInfo parse_solution(const fs::path& solution_path, const CliOptions& options, Diagnostics& diagnostics)
{
    std::ifstream file(solution_path);
    if (!file)
    {
        throw std::runtime_error("failed to open solution: " + path_string(solution_path));
    }

    SolutionInfo solution;
    solution.path = solution_path;
    solution.directory = absolute_normal(solution_path.parent_path());

    enum class Section
    {
        none,
        solution_configurations,
        legacy_solution_configurations,
        project_configurations
    } section = Section::none;

    std::string line;
    while (std::getline(file, line))
    {
        const std::string trimmed = trim(line);
        if (trimmed.rfind("Project(\"", 0) == 0)
        {
            std::vector<std::string> quoted;
            std::size_t pos = 0;
            while ((pos = trimmed.find('"', pos)) != std::string::npos)
            {
                const std::size_t end = trimmed.find('"', pos + 1);
                if (end == std::string::npos)
                {
                    break;
                }
                quoted.push_back(trimmed.substr(pos + 1, end - pos - 1));
                pos = end + 1;
            }
            if (quoted.size() >= 4)
            {
                SolutionProject project;
                project.name = quoted[1];
                project.path = absolute_normal(solution.directory / fs::path(quoted[2]));
                project.extension = to_lower(project.path.extension().string());
                project.project_guid = quoted[3];
                solution.projects.push_back(std::move(project));
            }
            continue;
        }

        if (trimmed.rfind("GlobalSection(SolutionConfigurationPlatforms)", 0) == 0)
        {
            section = Section::solution_configurations;
            continue;
        }
        if (trimmed.rfind("GlobalSection(SolutionConfiguration)", 0) == 0)
        {
            section = Section::legacy_solution_configurations;
            continue;
        }
        if (trimmed.rfind("GlobalSection(ProjectConfigurationPlatforms)", 0) == 0)
        {
            section = Section::project_configurations;
            continue;
        }
        if (trimmed == "EndGlobalSection")
        {
            section = Section::none;
            continue;
        }

        if (section == Section::solution_configurations || section == Section::legacy_solution_configurations)
        {
            const std::size_t equals = trimmed.find('=');
            const std::string key = trim(trimmed.substr(0, equals));
            if (auto pair = parse_pair(key))
            {
                solution.configurations.push_back(*pair);
            }
            continue;
        }

        if (section == Section::project_configurations)
        {
            const std::size_t equals = trimmed.find('=');
            if (equals == std::string::npos)
            {
                continue;
            }
            const std::string key = trim(trimmed.substr(0, equals));
            const std::string value = trim(trimmed.substr(equals + 1));
            const std::size_t guid_end = key.find('}');
            if (guid_end == std::string::npos || guid_end + 2 >= key.size() || key[guid_end + 1] != '.')
            {
                continue;
            }

            const std::string guid = key.substr(0, guid_end + 1);
            const std::string rest = key.substr(guid_end + 2);
            const std::string active_suffix = ".ActiveCfg";
            const std::string build_suffix = ".Build.0";
            if (rest.size() > active_suffix.size() && rest.compare(rest.size() - active_suffix.size(), active_suffix.size(), active_suffix) == 0)
            {
                const std::string solution_pair_key = rest.substr(0, rest.size() - active_suffix.size());
                if (auto active_pair = parse_pair(value))
                {
                    solution.project_configurations[guid][solution_pair_key].active_cfg = *active_pair;
                }
            }
            else if (rest.size() > build_suffix.size() && rest.compare(rest.size() - build_suffix.size(), build_suffix.size(), build_suffix) == 0)
            {
                const std::string solution_pair_key = rest.substr(0, rest.size() - build_suffix.size());
                solution.project_configurations[guid][solution_pair_key].build = true;
            }
        }
    }

    diagnostics.debug(options, "parsed solution projects: " + std::to_string(solution.projects.size()));
    return solution;
}

std::vector<ProjectSelection> select_solution_projects(const SolutionInfo& solution, const ConfigurationPair& solution_pair, const CliOptions& options, Diagnostics& diagnostics)
{
    std::vector<ProjectSelection> selections;
    const std::string solution_key = pair_to_string(solution_pair);
    for (const SolutionProject& project : solution.projects)
    {
        if (project.extension != ".vcxproj")
        {
            ++diagnostics.skipped_projects;
            diagnostics.verbose(options, "skipped project: " + path_string(project.path.filename()) + " is not a .vcxproj project");
            continue;
        }
        std::error_code ec;
        if (!fs::exists(project.path, ec))
        {
            ++diagnostics.skipped_projects;
            diagnostics.verbose(options, "skipped project: " + path_string(project.path) + " does not exist");
            continue;
        }

        const auto project_it = solution.project_configurations.find(project.project_guid);
        const auto mapping_it = project_it == solution.project_configurations.end() ? std::map<std::string, ProjectConfigurationMapping>::const_iterator{} : project_it->second.find(solution_key);
        if (project_it == solution.project_configurations.end() || mapping_it == project_it->second.end() || !mapping_it->second.active_cfg)
        {
            ++diagnostics.skipped_projects;
            diagnostics.verbose(options, "skipped: " + path_string(project.path.filename()) + " has no ActiveCfg for " + solution_key);
            continue;
        }
        if (!mapping_it->second.build)
        {
            ++diagnostics.skipped_projects;
            diagnostics.verbose(options, "skipped: " + path_string(project.path.filename()) + " is not selected for build under " + solution_key);
            continue;
        }

        selections.push_back(ProjectSelection{project.path, project.name, solution_pair, *mapping_it->second.active_cfg});
        diagnostics.verbose(options, "selected project: " + path_string(project.path.filename()) + " " + pair_to_string(*mapping_it->second.active_cfg));
    }
    return selections;
}

std::vector<ProjectSelection> select_input_projects(const fs::path& msbuild, const CliOptions& options, Diagnostics& diagnostics)
{
    const std::string extension = to_lower(options.input_path.extension().string());
    if (extension == ".sln")
    {
        SolutionInfo solution = parse_solution(options.input_path, options, diagnostics);
        if (solution.configurations.empty())
        {
            throw std::runtime_error("no solution configurations were found in " + path_string(options.input_path));
        }
        const auto solution_pair = select_configuration_pair(solution.configurations, options);
        if (!solution_pair)
        {
            throw std::runtime_error("solution configuration was not found.\n" + available_pairs_message(solution.configurations));
        }
        diagnostics.verbose(options, "selected solution configuration: " + pair_to_string(*solution_pair));
        std::vector<ProjectSelection> selections = select_solution_projects(solution, *solution_pair, options, diagnostics);
        if (selections.empty())
        {
            throw std::runtime_error("no .vcxproj projects were selected in " + path_string(options.input_path));
        }
        return selections;
    }

    std::vector<ConfigurationPair> pairs = query_project_configurations(msbuild, options.input_path, options, diagnostics);
    if (pairs.empty())
    {
        throw std::runtime_error("no ProjectConfiguration items were found in " + path_string(options.input_path));
    }
    const auto project_pair = select_configuration_pair(pairs, options);
    if (!project_pair)
    {
        throw std::runtime_error("configuration/platform was not found.\n" + available_pairs_message(pairs));
    }
    diagnostics.verbose(options, "selected project configuration: " + pair_to_string(*project_pair));
    return {ProjectSelection{options.input_path, options.input_path.stem().string(), *project_pair, *project_pair}};
}

bool contains_inheritance_marker(const std::string& value)
{
    return value.find("%(") != std::string::npos;
}

bool contains_unresolved_macro(const std::string& value)
{
    return value.find("$(") != std::string::npos || value.find("%(") != std::string::npos;
}

std::vector<std::string> split_msbuild_list(const std::string& value)
{
    std::vector<std::string> result;
    for (std::string part : split(value, ';'))
    {
        part = trim(part);
        if (part.empty() || contains_inheritance_marker(part))
        {
            continue;
        }
        if (part.size() >= 2 && part.front() == '"' && part.back() == '"')
        {
            part = part.substr(1, part.size() - 2);
        }
        result.push_back(std::move(part));
    }
    return result;
}

std::vector<std::string> split_windows_command_line(const std::string& value)
{
    std::vector<std::string> args;
    std::string current;
    bool in_quotes = false;
    std::size_t backslashes = 0;

    for (const char c : value)
    {
        if (c == '\\')
        {
            ++backslashes;
            continue;
        }
        if (c == '"')
        {
            current.append(backslashes / 2, '\\');
            if (backslashes % 2 == 0)
            {
                in_quotes = !in_quotes;
            }
            else
            {
                current.push_back('"');
            }
            backslashes = 0;
            continue;
        }
        current.append(backslashes, '\\');
        backslashes = 0;
        if (std::isspace(static_cast<unsigned char>(c)) && !in_quotes)
        {
            if (!current.empty())
            {
                args.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(c);
    }
    current.append(backslashes, '\\');
    if (!current.empty())
    {
        args.push_back(current);
    }
    return args;
}

bool is_c_source_extension(const fs::path& file)
{
    return to_lower(file.extension().string()) == ".c";
}

std::string compile_as_option(const ClCompileItem& item)
{
    const std::string compile_as = metadata_or_empty(item, "CompileAs");
    if (iequals(compile_as, "CompileAsC"))
    {
        return "/TC";
    }
    if (iequals(compile_as, "CompileAsCpp"))
    {
        return "/TP";
    }
    return is_c_source_extension(item.full_path) ? "/TC" : "/TP";
}

std::optional<std::string> language_standard_option(const ClCompileItem& item)
{
    const bool c_source = is_c_source_extension(item.full_path);
    const std::string standard = c_source ? metadata_or_empty(item, "LanguageStandard_C") : metadata_or_empty(item, "LanguageStandard");
    if (standard.empty() || iequals(standard, "Default"))
    {
        return std::nullopt;
    }

    static const std::map<std::string, std::string> mapping{{"stdcpp14", "/std:c++14"}, {"stdcpp17", "/std:c++17"},
                                                           {"stdcpp20", "/std:c++20"}, {"stdcpplatest", "/std:c++latest"},
                                                           {"stdc11", "/std:c11"},     {"stdc17", "/std:c17"},
                                                           {"stdclatest", "/std:clatest"}};
    const auto it = mapping.find(to_lower(standard));
    return it == mapping.end() ? std::nullopt : std::optional<std::string>(it->second);
}

std::string compiler_executable(const std::map<std::string, std::string>& properties)
{
    const std::string tool_path = value_or_empty(properties, "CLToolPath");
    const std::string tool_exe = value_or_empty(properties, "CLToolExe");
    if (!tool_path.empty() && !tool_exe.empty())
    {
        return path_string(fs::path(tool_path) / tool_exe);
    }
    if (!tool_exe.empty())
    {
        return tool_exe;
    }
    return to_lower(value_or_empty(properties, "PlatformToolset")).find("clang") != std::string::npos ? "clang-cl.exe" : "cl.exe";
}

std::optional<std::string> warning_level_option(const std::string& value)
{
    if (iequals(value, "Level1"))
    {
        return "/W1";
    }
    if (iequals(value, "Level2"))
    {
        return "/W2";
    }
    if (iequals(value, "Level3"))
    {
        return "/W3";
    }
    if (iequals(value, "Level4"))
    {
        return "/W4";
    }
    return std::nullopt;
}

std::optional<std::string> runtime_library_option(const std::string& value)
{
    if (iequals(value, "MultiThreadedDLL"))
    {
        return "/MD";
    }
    if (iequals(value, "MultiThreadedDebugDLL"))
    {
        return "/MDd";
    }
    if (iequals(value, "MultiThreaded"))
    {
        return "/MT";
    }
    if (iequals(value, "MultiThreadedDebug"))
    {
        return "/MTd";
    }
    return std::nullopt;
}

std::optional<std::string> optimization_option(const std::string& value)
{
    if (iequals(value, "Disabled"))
    {
        return "/Od";
    }
    if (iequals(value, "MaxSpeed"))
    {
        return "/O2";
    }
    if (iequals(value, "MinSpace"))
    {
        return "/O1";
    }
    return std::nullopt;
}

std::optional<std::string> debug_information_option(const std::string& value)
{
    if (iequals(value, "ProgramDatabase"))
    {
        return "/Zi";
    }
    if (iequals(value, "OldStyle"))
    {
        return "/Z7";
    }
    return std::nullopt;
}

std::optional<std::string> exception_handling_option(const std::string& value)
{
    if (iequals(value, "Sync"))
    {
        return "/EHsc";
    }
    if (iequals(value, "Async"))
    {
        return "/EHa";
    }
    return std::nullopt;
}

std::optional<std::string> runtime_type_info_option(const std::string& value)
{
    if (iequals(value, "true"))
    {
        return "/GR";
    }
    if (iequals(value, "false"))
    {
        return "/GR-";
    }
    return std::nullopt;
}

std::optional<std::string> precompiled_header_option(const std::string& value)
{
    if (iequals(value, "Use"))
    {
        return "/Yu";
    }
    if (iequals(value, "Create"))
    {
        return "/Yc";
    }
    return std::nullopt;
}

fs::path evaluated_project_directory(const EvaluatedProject& project)
{
    const std::string project_dir = value_or_empty(project.properties, "ProjectDir");
    const std::string msbuild_project_dir = value_or_empty(project.properties, "MSBuildProjectDirectory");
    if (!project_dir.empty())
    {
        return absolute_normal(project_dir);
    }
    if (!msbuild_project_dir.empty())
    {
        return absolute_normal(msbuild_project_dir);
    }
    return project.project_directory;
}

void add_include_paths(CompilerOptions& options, const fs::path& directory, const std::string& raw, const CliOptions& cli_options, Diagnostics& diagnostics, const fs::path& project_path)
{
    for (const std::string& include : split_msbuild_list(raw))
    {
        if (contains_unresolved_macro(include))
        {
            diagnostics.debug(cli_options, "unresolved macro in include directory for " + path_string(project_path) + ": " + include);
            continue;
        }
        const fs::path include_path = resolve_against(directory, include);
        if (std::find(options.include_directories.begin(), options.include_directories.end(), include_path) == options.include_directories.end())
        {
            options.include_directories.push_back(include_path);
        }
    }
}

std::string remove_inherited_additional_options(std::string value)
{
    for (const std::string& marker : {"%(AdditionalOptions)", "%(ClCompile.AdditionalOptions)"})
    {
        std::size_t pos = std::string::npos;
        while ((pos = value.find(marker)) != std::string::npos)
        {
            value.erase(pos, marker.size());
        }
    }
    return value;
}

std::optional<CompileFile> translate_item_to_document_file(const EvaluatedProject& project, const ClCompileItem& item, const CliOptions& options, Diagnostics& diagnostics)
{
    if (is_truthy_msbuild(metadata_or_empty(item, "ExcludedFromBuild")))
    {
        ++diagnostics.skipped_files;
        diagnostics.verbose(options, "skipped file: " + path_string(item.full_path) + " is excluded from build in " + pair_to_string(project.project_pair));
        return std::nullopt;
    }

    const fs::path directory = evaluated_project_directory(project);
    const fs::path file = item.full_path.empty() ? resolve_against(project.project_directory, item.identity) : absolute_normal(item.full_path);
    if (file.empty())
    {
        ++diagnostics.skipped_files;
        diagnostics.warning("skipped ClCompile item without FullPath or Identity in " + path_string(project.project_path));
        return std::nullopt;
    }

    std::error_code ec;
    if (!fs::exists(file, ec))
    {
        diagnostics.warning("source " + path_string(file) + " does not exist yet; entry was still emitted");
    }

    CompileFile document_file;
    document_file.directory = directory;
    document_file.file = file;
    document_file.options.compiler = compiler_executable(project.properties);
    document_file.options.compile_as = compile_as_option(item);
    document_file.options.language_standard = language_standard_option(item);

    add_include_paths(document_file.options, directory, metadata_or_empty(item, "AdditionalIncludeDirectories"), options, diagnostics, project.project_path);
    add_include_paths(document_file.options, directory, value_or_empty(project.properties, "ExternalIncludePath"), options, diagnostics, project.project_path);
    add_include_paths(document_file.options, directory, value_or_empty(project.properties, "IncludePath"), options, diagnostics, project.project_path);

    document_file.options.definitions = split_msbuild_list(metadata_or_empty(item, "PreprocessorDefinitions"));
    document_file.options.undefinitions = split_msbuild_list(metadata_or_empty(item, "UndefinePreprocessorDefinitions"));

    for (const std::string& forced_include : split_msbuild_list(metadata_or_empty(item, "ForcedIncludeFiles")))
    {
        if (contains_unresolved_macro(forced_include))
        {
            diagnostics.debug(options, "unresolved macro in forced include for " + path_string(project.project_path) + ": " + forced_include);
            continue;
        }
        document_file.options.forced_include_files.push_back(resolve_against(directory, forced_include));
    }

    document_file.options.warning_level = warning_level_option(metadata_or_empty(item, "WarningLevel"));
    document_file.options.treat_warning_as_error = is_truthy_msbuild(metadata_or_empty(item, "TreatWarningAsError"));
    document_file.options.runtime_library = runtime_library_option(metadata_or_empty(item, "RuntimeLibrary"));
    document_file.options.optimization = optimization_option(metadata_or_empty(item, "Optimization"));
    document_file.options.debug_information_format = debug_information_option(metadata_or_empty(item, "DebugInformationFormat"));
    document_file.options.exception_handling = exception_handling_option(metadata_or_empty(item, "ExceptionHandling"));
    document_file.options.runtime_type_info = runtime_type_info_option(metadata_or_empty(item, "RuntimeTypeInfo"));
    document_file.options.disabled_warnings = split_msbuild_list(metadata_or_empty(item, "DisableSpecificWarnings"));
    document_file.options.precompiled_header_mode = precompiled_header_option(metadata_or_empty(item, "PrecompiledHeader"));

    const std::string pch_file = metadata_or_empty(item, "PrecompiledHeaderFile");
    if (!pch_file.empty())
    {
        document_file.options.precompiled_header_file = pch_file;
    }

    document_file.options.additional_options = split_windows_command_line(remove_inherited_additional_options(metadata_or_empty(item, "AdditionalOptions")));

    const std::string object_file = metadata_or_empty(item, "ObjectFileName");
    if (!object_file.empty() && !contains_unresolved_macro(object_file))
    {
        document_file.output = resolve_against(directory, object_file);
    }
    return document_file;
}

CompilationDocument translate_project_to_document(const EvaluatedProject& project, const CliOptions& options, Diagnostics& diagnostics)
{
    CompilationDocument document;
    for (const ClCompileItem& item : project.cl_compile_items)
    {
        if (auto file = translate_item_to_document_file(project, item, options, diagnostics))
        {
            document.files.push_back(std::move(*file));
        }
    }
    return document;
}

std::vector<EvaluatedProject> evaluate_projects(const fs::path& msbuild, const std::vector<ProjectSelection>& selections, const CliOptions& options, Diagnostics& diagnostics, bool& strict_failure)
{
    std::vector<EvaluatedProject> evaluated;
    std::vector<std::future<std::optional<EvaluatedProject>>> futures;
    std::size_t next = 0;
    strict_failure = false;

    auto launch = [&](const ProjectSelection& selection) {
        return std::async(std::launch::async, [&msbuild, &selection, &options, &diagnostics]() -> std::optional<EvaluatedProject> {
            try
            {
                return evaluate_project(msbuild, selection, options, diagnostics);
            }
            catch (const std::exception& ex)
            {
                if (options.strict)
                {
                    throw;
                }
                ++diagnostics.skipped_projects;
                diagnostics.warning("skipped " + path_string(selection.project_path) + " because MSBuild evaluation failed: " + ex.what());
                return std::nullopt;
            }
        });
    };

    while (next < selections.size() || !futures.empty())
    {
        while (next < selections.size() && futures.size() < options.jobs)
        {
            futures.push_back(launch(selections[next++]));
        }
        try
        {
            auto result = futures.front().get();
            if (result)
            {
                evaluated.push_back(std::move(*result));
            }
        }
        catch (const std::exception& ex)
        {
            strict_failure = true;
            diagnostics.warning(std::string("project evaluation failed in strict mode: ") + ex.what());
        }
        futures.erase(futures.begin());
        if (strict_failure)
        {
            break;
        }
    }
    return evaluated;
}
} // namespace

DocumentCreationResult MsbuildDocumentCreator::create(const CliOptions& options, Diagnostics& diagnostics) const
{
    const fs::path msbuild = locate_msbuild(options);
    validate_msbuild_version(msbuild, options, diagnostics);

    const std::vector<ProjectSelection> selections = select_input_projects(msbuild, options, diagnostics);
    bool strict_failure = false;
    const std::vector<EvaluatedProject> evaluated_projects = evaluate_projects(msbuild, selections, options, diagnostics, strict_failure);

    DocumentCreationResult result;
    result.strict_failure = strict_failure;
    result.evaluated_projects = static_cast<unsigned>(evaluated_projects.size());
    for (const EvaluatedProject& project : evaluated_projects)
    {
        CompilationDocument project_document = translate_project_to_document(project, options, diagnostics);
        result.document.files.insert(result.document.files.end(), std::make_move_iterator(project_document.files.begin()), std::make_move_iterator(project_document.files.end()));
    }
    return result;
}
} // namespace vscc
