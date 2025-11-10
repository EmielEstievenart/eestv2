#include "eestv/data_bridge/data_bridge.hpp"
#include "eestv/data_bridge/data_bridge_cli.hpp"
#include "eestv/logging/eestv_logging.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/program_options/errors.hpp>
#include <gtest/gtest.h>

#include <initializer_list>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace
{
const auto wait_for_async = std::chrono::milliseconds(50);

class LogLevelGuard
{
public:
    LogLevelGuard() : _previous(eestv::logging::current_log_level) { }
    LogLevelGuard(const LogLevelGuard&)            = delete;
    LogLevelGuard& operator=(const LogLevelGuard&) = delete;
    LogLevelGuard(LogLevelGuard&&)                 = delete;
    LogLevelGuard& operator=(LogLevelGuard&&)      = delete;

    ~LogLevelGuard() { eestv::logging::current_log_level = _previous; }

private:
    eestv::logging::LogLevel _previous;
};

class DataBridgeCliTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        EESTV_SET_LOG_LEVEL(Trace);

        io_context = std::make_unique<boost::asio::io_context>();
        // Keep io_context alive until explicitly stopped
        work_guard = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(io_context->get_executor());

        io_thread = std::thread(
            [this]()
            {
                try
                {
                    io_context->run();
                }
                catch (const std::exception& e)
                {
                    EESTV_LOG_ERROR("Exception in io_context.run(): " << e.what());
                }
                catch (...)
                {
                    EESTV_LOG_ERROR("Unknown exception in io_context.run()");
                }

                EESTV_LOG_DEBUG("io_context returned");
            });

        std::this_thread::sleep_for(wait_for_async);
    }

    void TearDown() override
    {
        work_guard.reset();
        io_context->stop();

        if (io_thread.joinable())
        {
            io_thread.join();
        }
    }

    std::unique_ptr<boost::asio::io_context> io_context;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard;
    std::thread io_thread;
};

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
            _argv.emplace_back(argument.data());
        }
    }

    [[nodiscard]] int argc() const noexcept { return static_cast<int>(_argv.size()); }

    [[nodiscard]] char** argv() noexcept { return _argv.data(); }

private:
    std::vector<std::string> _storage;
    std::vector<char*> _argv;
};
} // namespace

TEST_F(DataBridgeCliTest, ParsesEndpointWithInfoVerbosity)
{
    LogLevelGuard guard;
    eestv::logging::current_log_level = eestv::logging::LogLevel::Trace;

    ArgumentBuffer arguments {"data_bridge", "--endpoint", "--discovery", "alpha"};

    auto config = eestv::parse_command_line(arguments.argc(), arguments.argv());
    eestv::DataBridge data_bridge(config, *io_context);

    EXPECT_EQ(eestv::EndpointMode::endpoint, data_bridge.endpoint_mode());
    EXPECT_EQ("alpha", data_bridge.discovery_target());
    EXPECT_EQ(eestv::logging::LogLevel::Info, config.log_level);
}

TEST_F(DataBridgeCliTest, ParsesDebugVerbosity)
{
    LogLevelGuard guard;
    eestv::logging::current_log_level = eestv::logging::LogLevel::Error;

    ArgumentBuffer arguments {"data_bridge", "-v", "--endpoint", "--discovery", "beta"};

    auto config = eestv::parse_command_line(arguments.argc(), arguments.argv());
    eestv::DataBridge data_bridge(config, *io_context);

    EXPECT_EQ(eestv::EndpointMode::endpoint, data_bridge.endpoint_mode());
    EXPECT_EQ("beta", data_bridge.discovery_target());
    EXPECT_EQ(eestv::logging::LogLevel::Debug, config.log_level);
}

TEST_F(DataBridgeCliTest, ParsesTraceVerbosityAndBridgeMode)
{
    LogLevelGuard guard;
    eestv::logging::current_log_level = eestv::logging::LogLevel::Error;

    ArgumentBuffer arguments {"data_bridge", "-vv", "--bridge", "--discovery", "gamma"};

    auto config = eestv::parse_command_line(arguments.argc(), arguments.argv());
    eestv::DataBridge data_bridge(config, *io_context);

    EXPECT_EQ(eestv::EndpointMode::bridge, data_bridge.endpoint_mode());
    EXPECT_EQ("gamma", data_bridge.discovery_target());
    EXPECT_EQ(eestv::logging::LogLevel::Trace, config.log_level);
}

TEST_F(DataBridgeCliTest, ThrowsWhenBothEndpointAndBridge)
{
    LogLevelGuard guard;

    ArgumentBuffer arguments {"data_bridge", "--endpoint", "--bridge", "--discovery", "epsilon"};

    EXPECT_THROW(eestv::parse_command_line(arguments.argc(), arguments.argv()), boost::program_options::error);
}

TEST_F(DataBridgeCliTest, ThrowsWhenDiscoveryMissing)
{
    LogLevelGuard guard;

    ArgumentBuffer arguments {"data_bridge", "--endpoint"};

    EXPECT_THROW(eestv::parse_command_line(arguments.argc(), arguments.argv()), boost::program_options::error);
}

TEST_F(DataBridgeCliTest, DiscoveryBetweenTwoDataBridges)
{
    LogLevelGuard guard;
    eestv::logging::current_log_level = eestv::logging::LogLevel::Trace;

    // Create two data bridges with the same discovery target but different ports
    ArgumentBuffer arguments1 {"data_bridge", "--endpoint", "-vv", "--discovery", "test_group"};
    ArgumentBuffer arguments2 {"data_bridge", "--endpoint", "-vv", "--discovery", "test_group"};

    auto config1 = eestv::parse_command_line(arguments1.argc(), arguments1.argv());
    auto config2 = eestv::parse_command_line(arguments2.argc(), arguments2.argv());

    eestv::logging::current_log_level = config1.log_level;

    eestv::DataBridge data_bridge1(config1, *io_context, eestv::DataBridge::default_port);
    eestv::DataBridge data_bridge2(config2, *io_context, eestv::DataBridge::default_port);

    // Let them discover each other for a couple of seconds
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Query their counters - both should have discovered each other
    EXPECT_GT(data_bridge1.discovery_count(), 0U);
    EXPECT_GT(data_bridge2.discovery_count(), 0U);

    // Both should have been discovered at least once
    EXPECT_GT(data_bridge1.discovered_count(), 0U);
    EXPECT_GT(data_bridge2.discovered_count(), 0U);
}