#include <boost/program_options/errors.hpp>
#include <gtest/gtest.h>

#include <initializer_list>
#include <string>
#include <vector>

#include "command_line_parser.hpp"

namespace slayerlog
{

namespace
{

class ArgumentBuffer
{
public:
    ArgumentBuffer(std::initializer_list<const char*> arguments)
    {
        _storage.reserve(arguments.size());
        for (const char* item : arguments)
        {
            _storage.emplace_back(item);
        }

        _argv.reserve(_storage.size());
        for (std::string& argument : _storage)
        {
            _argv.push_back(argument.data());
        }
    }

    [[nodiscard]] int argc() const noexcept { return static_cast<int>(_argv.size()); }

    [[nodiscard]] char** argv() noexcept { return _argv.data(); }

private:
    std::vector<std::string> _storage;
    std::vector<char*> _argv;
};

} // namespace

TEST(CommandLineParserTest, AllowsStartingWithoutAnyFiles)
{
    ArgumentBuffer arguments {"slayerlog"};

    const auto config = parse_command_line(arguments.argc(), arguments.argv());

    EXPECT_TRUE(config.file_paths.empty());
    EXPECT_EQ(config.poll_interval_ms, 250);
}

TEST(CommandLineParserTest, ParsesProvidedFilesAndPollInterval)
{
    ArgumentBuffer arguments {"slayerlog", "--file", "a.log", "--file", "b.log", "--poll-interval-ms", "125"};

    const auto config = parse_command_line(arguments.argc(), arguments.argv());

    ASSERT_EQ(config.file_paths.size(), 2U);
    EXPECT_EQ(config.file_paths[0], "a.log");
    EXPECT_EQ(config.file_paths[1], "b.log");
    EXPECT_EQ(config.poll_interval_ms, 125);
}

TEST(CommandLineParserTest, ParsesPositionalFiles)
{
    ArgumentBuffer arguments {"slayerlog", "first.log", "second.log"};

    const auto config = parse_command_line(arguments.argc(), arguments.argv());

    ASSERT_EQ(config.file_paths.size(), 2U);
    EXPECT_EQ(config.file_paths[0], "first.log");
    EXPECT_EQ(config.file_paths[1], "second.log");
}

TEST(CommandLineParserTest, ThrowsOnNonPositivePollInterval)
{
    ArgumentBuffer arguments {"slayerlog", "--poll-interval-ms", "0"};

    EXPECT_THROW(parse_command_line(arguments.argc(), arguments.argv()), boost::program_options::error);
}

} // namespace slayerlog
