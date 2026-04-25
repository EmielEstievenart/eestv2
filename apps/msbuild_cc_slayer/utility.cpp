#include "utility.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <sstream>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace vscc
{
std::string trim(const std::string& text)
{
    const auto begin = std::find_if_not(text.begin(), text.end(), [](unsigned char c) { return std::isspace(c); });
    const auto end = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char c) { return std::isspace(c); }).base();
    return begin >= end ? std::string{} : std::string(begin, end);
}

std::string to_lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

bool iequals(const std::string& lhs, const std::string& rhs)
{
    return to_lower(lhs) == to_lower(rhs);
}

std::vector<std::string> split(const std::string& text, char delimiter)
{
    std::vector<std::string> parts;
    std::string current;
    std::istringstream stream(text);
    while (std::getline(stream, current, delimiter))
    {
        parts.push_back(current);
    }
    if (!text.empty() && text.back() == delimiter)
    {
        parts.emplace_back();
    }
    return parts;
}

std::optional<ConfigurationPair> parse_pair(const std::string& text)
{
    const std::size_t pipe = text.find('|');
    if (pipe == std::string::npos)
    {
        return std::nullopt;
    }
    return ConfigurationPair{trim(text.substr(0, pipe)), trim(text.substr(pipe + 1))};
}

std::string pair_to_string(const ConfigurationPair& pair)
{
    return pair.configuration + "|" + pair.platform;
}

std::string path_string(const fs::path& path)
{
    return path.lexically_normal().string();
}

fs::path absolute_normal(const fs::path& path)
{
    std::error_code ec;
    fs::path result = path.is_absolute() ? path : fs::absolute(path, ec);
    return (ec ? path : result).lexically_normal();
}

fs::path resolve_against(const fs::path& base, const std::string& value)
{
    fs::path path(value);
    return path.is_absolute() ? path.lexically_normal() : (base / path).lexically_normal();
}

bool is_truthy_msbuild(const std::string& value)
{
    return iequals(trim(value), "true");
}

std::string json_escape(const std::string& value)
{
    std::ostringstream stream;
    for (const char c : value)
    {
        switch (c)
        {
        case '"': stream << "\\\""; break;
        case '\\': stream << "\\\\"; break;
        case '\b': stream << "\\b"; break;
        case '\f': stream << "\\f"; break;
        case '\n': stream << "\\n"; break;
        case '\r': stream << "\\r"; break;
        case '\t': stream << "\\t"; break;
        default: stream << c; break;
        }
    }
    return stream.str();
}

std::string quote_command_arg(const std::string& arg)
{
    if (arg.empty())
    {
        return "\"\"";
    }
    if (arg.find_first_of(" \t\"&()[]{}^=;!'+,`~") == std::string::npos)
    {
        return arg;
    }

    std::string result = "\"";
    std::size_t backslashes = 0;
    for (const char c : arg)
    {
        if (c == '\\')
        {
            ++backslashes;
            continue;
        }
        if (c == '"')
        {
            result.append(backslashes * 2 + 1, '\\');
            result.push_back(c);
            backslashes = 0;
            continue;
        }
        result.append(backslashes, '\\');
        backslashes = 0;
        result.push_back(c);
    }
    result.append(backslashes * 2, '\\');
    result.push_back('"');
    return result;
}

std::string command_line_for_debug(const std::vector<std::string>& args)
{
    std::ostringstream stream;
    for (std::size_t i = 0; i < args.size(); ++i)
    {
        if (i != 0)
        {
            stream << ' ';
        }
        stream << quote_command_arg(args[i]);
    }
    return stream.str();
}

ProcessResult run_process(const std::vector<std::string>& args)
{
    std::ostringstream command;
#if defined(_WIN32)
    command << "call ";
#endif
    for (std::size_t i = 0; i < args.size(); ++i)
    {
        if (i != 0)
        {
            command << ' ';
        }
        command << quote_command_arg(args[i]);
    }
    command << " 2>&1";

#if defined(_WIN32)
    FILE* pipe = _popen(command.str().c_str(), "r");
#else
    FILE* pipe = popen(command.str().c_str(), "r");
#endif
    if (pipe == nullptr)
    {
        return ProcessResult{1, "failed to start process"};
    }

    std::array<char, 4096> buffer{};
    std::string output;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
    {
        output += buffer.data();
    }

#if defined(_WIN32)
    return ProcessResult{_pclose(pipe), output};
#else
    const int status = pclose(pipe);
    return ProcessResult{WIFEXITED(status) ? WEXITSTATUS(status) : status, output};
#endif
}
} // namespace vscc
