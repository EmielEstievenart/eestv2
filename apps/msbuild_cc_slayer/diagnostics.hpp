#pragma once

#include "cli.hpp"

#include <iostream>
#include <mutex>
#include <string>

namespace vscc
{
struct Diagnostics
{
    std::mutex mutex;
    unsigned warnings = 0;
    unsigned skipped_projects = 0;
    unsigned skipped_files = 0;

    void warning(const std::string& message)
    {
        std::lock_guard<std::mutex> lock(mutex);
        ++warnings;
        std::cerr << "warning: " << message << '\n';
    }

    void verbose(const CliOptions& options, const std::string& message)
    {
        if (!options.verbose && !options.debug)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex);
        std::cout << message << '\n';
    }

    void debug(const CliOptions& options, const std::string& message)
    {
        if (!options.debug)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex);
        std::cout << "debug: " << message << '\n';
    }
};
} // namespace vscc
