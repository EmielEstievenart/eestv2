#include "compile_commands_printer.hpp"

#include "utility.hpp"

#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace vscc
{
namespace
{
struct CompileCommand
{
    std::string directory;
    std::string file;
    std::optional<std::string> output;
    std::vector<std::string> arguments;
};

void append_optional(std::vector<std::string>& args, const std::optional<std::string>& value)
{
    if (value && !value->empty())
    {
        args.push_back(*value);
    }
}

std::vector<std::string> build_arguments(const CompileFile& file)
{
    std::vector<std::string> args;
    const CompilerOptions& options = file.options;
    args.push_back(options.compiler.empty() ? "cl.exe" : options.compiler);
    args.push_back("/nologo");
    args.push_back(options.compile_as.empty() ? "/TP" : options.compile_as);

    for (const fs::path& include : options.include_directories)
    {
        args.push_back("/I");
        args.push_back(path_string(include));
    }
    for (const std::string& definition : options.definitions)
    {
        args.push_back("/D" + definition);
    }
    for (const std::string& undefinition : options.undefinitions)
    {
        args.push_back("/U" + undefinition);
    }
    for (const fs::path& forced_include : options.forced_include_files)
    {
        args.push_back("/FI" + path_string(forced_include));
    }

    append_optional(args, options.language_standard);
    append_optional(args, options.warning_level);
    if (options.treat_warning_as_error)
    {
        args.push_back("/WX");
    }
    append_optional(args, options.runtime_library);
    append_optional(args, options.optimization);
    append_optional(args, options.debug_information_format);
    append_optional(args, options.exception_handling);
    append_optional(args, options.runtime_type_info);

    for (const std::string& warning : options.disabled_warnings)
    {
        args.push_back("/wd" + warning);
    }
    if (options.precompiled_header_mode && options.precompiled_header_file)
    {
        args.push_back(*options.precompiled_header_mode + *options.precompiled_header_file);
    }
    for (const std::string& option : options.additional_options)
    {
        args.push_back(option);
    }

    args.push_back("/c");
    args.push_back(path_string(file.file));
    return args;
}

std::string command_key(const CompileCommand& command)
{
    std::ostringstream stream;
    stream << command.directory << '\0' << command.file << '\0';
    for (const std::string& argument : command.arguments)
    {
        stream << argument << '\0';
    }
    if (command.output)
    {
        stream << *command.output;
    }
    return stream.str();
}

std::vector<CompileCommand> to_commands(const CompilationDocument& document)
{
    std::vector<CompileCommand> commands;
    std::set<std::string> seen;
    for (const CompileFile& file : document.files)
    {
        CompileCommand command;
        command.directory = path_string(file.directory);
        command.file = path_string(file.file);
        if (file.output)
        {
            command.output = path_string(*file.output);
        }
        command.arguments = build_arguments(file);

        if (seen.insert(command_key(command)).second)
        {
            commands.push_back(std::move(command));
        }
    }
    return commands;
}
} // namespace

void CompileCommandsPrinter::write_file(const fs::path& output_path, const CompilationDocument& document) const
{
    fs::create_directories(output_path.parent_path());
    fs::path temp_path = output_path;
    temp_path += ".tmp";
    {
        std::ofstream output(temp_path, std::ios::binary);
        if (!output)
        {
            throw std::runtime_error("failed to open output file: " + path_string(temp_path));
        }
        write(output, document);
    }

    std::error_code ec;
    fs::remove(output_path, ec);
    fs::rename(temp_path, output_path, ec);
    if (ec)
    {
        throw std::runtime_error("failed to replace output file: " + ec.message());
    }
}

void CompileCommandsPrinter::write(std::ostream& stream, const CompilationDocument& document) const
{
    const std::vector<CompileCommand> commands = to_commands(document);
    stream << "[\n";
    for (std::size_t i = 0; i < commands.size(); ++i)
    {
        const CompileCommand& command = commands[i];
        stream << "  {\n";
        stream << "    \"directory\": \"" << json_escape(command.directory) << "\",\n";
        stream << "    \"file\": \"" << json_escape(command.file) << "\",\n";
        if (command.output)
        {
            stream << "    \"output\": \"" << json_escape(*command.output) << "\",\n";
        }
        stream << "    \"arguments\": [\n";
        for (std::size_t argument_index = 0; argument_index < command.arguments.size(); ++argument_index)
        {
            stream << "      \"" << json_escape(command.arguments[argument_index]) << "\"";
            if (argument_index + 1 != command.arguments.size())
            {
                stream << ',';
            }
            stream << '\n';
        }
        stream << "    ]\n";
        stream << "  }";
        if (i + 1 != commands.size())
        {
            stream << ',';
        }
        stream << '\n';
    }
    stream << "]\n";
}
} // namespace vscc
