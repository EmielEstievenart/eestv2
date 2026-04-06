#include "command_line_parser.hpp"

#include <boost/program_options.hpp>

#include <cstdlib>
#include <iostream>
#include <vector>

namespace slayerlog
{

Config parse_command_line(int argc, char* argv[])
{
    namespace po = boost::program_options;

    po::options_description desc("Slayerlog Options");
    // clang-format off
    desc.add_options()
        ("help,h", "Show help message")
        ("file,f", po::value<std::vector<std::string>>()->composing(), "Path to a log file to open on startup. Repeat for multiple files.")
        ("poll-interval-ms", po::value<int>()->default_value(250), "Polling interval in milliseconds");
    // clang-format on

    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i)
    {
        arguments.emplace_back(argv[i]);
    }

    po::positional_options_description positional;
    positional.add("file", -1);

    po::variables_map variables;

    try
    {
        auto parser = po::command_line_parser(arguments).options(desc).positional(positional);

        po::store(parser.run(), variables);
        po::notify(variables);

        if (variables.count("help") != 0U)
        {
            std::cout << desc << '\n';
            std::exit(0);
        }

        Config config;
        if (variables.count("file") != 0U)
        {
            config.file_paths = variables["file"].as<std::vector<std::string>>();
        }
        config.poll_interval_ms = variables["poll-interval-ms"].as<int>();

        if (config.poll_interval_ms <= 0)
        {
            throw po::error("--poll-interval-ms must be greater than 0");
        }

        return config;
    }
    catch (const po::error& error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        std::cerr << desc << '\n';
        throw;
    }
}

} // namespace slayerlog
