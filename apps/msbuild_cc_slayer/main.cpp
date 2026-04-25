#include "cli.hpp"
#include "compile_commands_printer.hpp"
#include "diagnostics.hpp"
#include "msbuild_document_creator.hpp"
#include "utility.hpp"

#include <exception>
#include <iostream>

namespace
{
constexpr int exit_success = 0;
constexpr int exit_error = 1;
constexpr int exit_no_commands = 2;
constexpr int exit_strict_failure = 3;
} // namespace

int main(int argc, char** argv)
{
    try
    {
        vscc::Diagnostics diagnostics;
        const vscc::CliOptions options = vscc::parse_cli(argc, argv);
        if (options.help)
        {
            vscc::print_help();
            return exit_success;
        }

        diagnostics.verbose(options, "input: " + vscc::path_string(options.input_path));

        const vscc::MsbuildDocumentCreator creator;
        const vscc::DocumentCreationResult result = creator.create(options, diagnostics);
        if (result.strict_failure)
        {
            return exit_strict_failure;
        }
        if (result.document.files.empty())
        {
            std::cerr << "error: no compile commands generated.\n";
            return exit_no_commands;
        }

        const vscc::CompileCommandsPrinter printer;
        if (options.dry_run)
        {
            std::cout << "dry-run: would write " << vscc::path_string(options.output_path) << '\n';
        }
        else
        {
            printer.write_file(options.output_path, result.document);
            std::cout << "wrote " << vscc::path_string(options.output_path) << '\n';
        }

        std::cout << "projects: " << result.evaluated_projects << " evaluated, " << diagnostics.skipped_projects << " skipped\n";
        std::cout << "files: " << result.document.files.size() << " compile commands\n";
        std::cout << "warnings: " << diagnostics.warnings << '\n';
        return exit_success;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "error: " << ex.what() << '\n';
        return exit_error;
    }
}
