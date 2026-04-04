#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace slayerlog
{

struct CommandDescriptor
{
    std::string name;
    std::string summary;
    std::string usage;
};

struct CommandResult
{
    bool success = false;
    std::string message;
};

using CommandHandler = std::function<CommandResult(std::string_view arguments)>;

class CommandManager
{
public:
    void register_command(CommandDescriptor descriptor, CommandHandler handler);

    std::vector<CommandDescriptor> matching_commands(std::string_view query) const;
    CommandResult execute(std::string_view command_line) const;

private:
    struct RegisteredCommand
    {
        CommandDescriptor descriptor;
        CommandHandler handler;
        std::string normalized_name;
    };

    static std::string normalize_command_name(std::string_view name);
    static std::string trim(std::string_view text);
    static std::string typed_command_name(std::string_view query);

    const RegisteredCommand* find_command(std::string_view name) const;

    std::vector<RegisteredCommand> _commands;
};

} // namespace slayerlog
