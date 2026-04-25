#include "cli.hpp"

#include "utility.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace vscc
{
CliOptions parse_cli(int argc, char** argv)
{
    CliOptions options;
    options.jobs = std::min(std::max(1u, std::thread::hardware_concurrency()), 4u);

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        auto require_value    = [&](const std::string& option) -> std::string
        {
            if (i + 1 >= argc)
            {
                throw std::runtime_error("missing value for " + option);
            }
            return argv[++i];
        };

        if (arg == "--help" || arg == "-h")
        {
            options.help = true;
        }
        else if (arg == "--configuration")
        {
            options.configuration = require_value(arg);
        }
        else if (arg == "--platform")
        {
            options.platform = require_value(arg);
        }
        else if (arg == "--output")
        {
            options.output_path = require_value(arg);
        }
        else if (arg == "--msbuild-path")
        {
            options.msbuild_path = fs::path(require_value(arg));
        }
        else if (arg == "--jobs")
        {
            options.jobs = static_cast<unsigned>(std::max(1, std::stoi(require_value(arg))));
        }
        else if (arg == "--verbose")
        {
            options.verbose = true;
        }
        else if (arg == "--debug")
        {
            options.debug   = true;
            options.verbose = true;
        }
        else if (arg == "--dry-run")
        {
            options.dry_run = true;
        }
        else if (arg == "--strict")
        {
            options.strict = true;
        }
        else if (!arg.empty() && arg.front() == '-')
        {
            throw std::runtime_error("unknown option: " + arg);
        }
        else if (options.input_path.empty())
        {
            options.input_path = arg;
        }
        else
        {
            throw std::runtime_error("unexpected argument: " + arg);
        }
    }

    if (options.help)
    {
        return options;
    }
    if (options.input_path.empty())
    {
        throw std::runtime_error("missing input file");
    }

    options.input_path = absolute_normal(options.input_path);
    std::error_code ec;
    if (!fs::exists(options.input_path, ec))
    {
        throw std::runtime_error("input file does not exist: " + path_string(options.input_path));
    }

    const std::string extension = to_lower(options.input_path.extension().string());
    if (extension != ".sln" && extension != ".vcxproj")
    {
        throw std::runtime_error("unsupported input extension '" + extension + "'. Expected .sln or .vcxproj.");
    }

    if (options.output_path.empty())
    {
        options.output_path = options.input_path.parent_path() / "compile_commands.json";
    }
    else
    {
        options.output_path = absolute_normal(options.output_path);
    }
    return options;
}

void print_help()
{
    std::cout << "Usage: vscompilecommands <input.sln|input.vcxproj> [options]\n\n"
              << "Options:\n"
              << "  --configuration <name>  Visual Studio configuration, e.g. Debug\n"
              << "  --platform <name>       Visual Studio platform, e.g. x64 or Win32\n"
              << "  --output <path>         Output path, defaults next to the input\n"
              << "  --msbuild-path <path>   Explicit MSBuild.exe path\n"
              << "  --jobs <n>              Project evaluation parallelism\n"
              << "  --verbose               Print selected/skipped projects\n"
              << "  --debug                 Print MSBuild commands and detailed diagnostics\n"
              << "  --dry-run               Evaluate but do not write compile_commands.json\n"
              << "  --strict                Treat project evaluation failures as fatal\n"
              << "  --help                  Show this help\n";
}
} // namespace vscc
