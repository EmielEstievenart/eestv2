#include "eestv/logging/eestv_logging.hpp"
#include "eestv/net/discovery/udp_discovery_server.hpp"
#include "eestv/net/discovery/discoverable.hpp"
#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>

namespace
{
const int test_port            = 54324;
const std::string test_service = "test_service";
const auto short_timeout       = std::chrono::milliseconds(100);
const auto medium_timeout      = std::chrono::milliseconds(500);
const auto wait_for_async      = std::chrono::milliseconds(50);
const auto wait_for_operation  = std::chrono::milliseconds(200);
} // namespace

using eestv::Discoverable;
using eestv::UdpDiscoveryServer;

/**
 * Lifecycle tests for UdpDiscoveryServer - tests creation, start, stop, and destruction
 */
class DiscoveryServerLifecycleTest : public ::testing::Test
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
        if (server)
        {
            server->stop();
            server.reset();
        }

        work_guard.reset();

        io_context->stop();

        if (io_thread.joinable())
        {
            io_thread.join();
        }
    }

    std::unique_ptr<boost::asio::io_context> io_context;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard;
    std::unique_ptr<UdpDiscoveryServer> server;
    std::thread io_thread;
};

TEST_F(DiscoveryServerLifecycleTest, Destruct)
{
    ASSERT_NO_THROW({ server = std::make_unique<UdpDiscoveryServer>(*io_context, test_port); });

    EXPECT_NE(server, nullptr);
}

TEST_F(DiscoveryServerLifecycleTest, StartAndDestruct)
{
    ASSERT_NO_THROW({ server = std::make_unique<UdpDiscoveryServer>(*io_context, test_port); });

    server->async_start();

    std::this_thread::sleep_for(wait_for_operation);

    EXPECT_NE(server, nullptr);
}

TEST_F(DiscoveryServerLifecycleTest, StartAndDestructImmediately)
{
    server = std::make_unique<UdpDiscoveryServer>(*io_context, test_port);

    ASSERT_NO_THROW(server->async_start());
}

TEST_F(DiscoveryServerLifecycleTest, StartStopAndDestruct)
{
    ASSERT_NO_THROW({ server = std::make_unique<UdpDiscoveryServer>(*io_context, test_port); });

    server->async_start();

    std::this_thread::sleep_for(wait_for_operation);

    server->stop();

    EXPECT_NE(server, nullptr);
}

TEST_F(DiscoveryServerLifecycleTest, StartStopImmediatelyAndDestruct)
{
    ASSERT_NO_THROW({ server = std::make_unique<UdpDiscoveryServer>(*io_context, test_port); });

    server->async_start();
    server->stop();

    EXPECT_NE(server, nullptr);
}

TEST_F(DiscoveryServerLifecycleTest, StartAndAsyncStop)
{
    std::atomic<bool> stopped {false};

    server = std::make_unique<UdpDiscoveryServer>(*io_context, test_port);

    server->async_start();
    std::this_thread::sleep_for(wait_for_async);

    bool stop_initiated = server->async_stop([&stopped]() { stopped = true; });

    EXPECT_TRUE(stop_initiated);

    auto start_time = std::chrono::steady_clock::now();
    while (!stopped && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2))
    {
        std::this_thread::sleep_for(wait_for_async);
    }

    EXPECT_TRUE(stopped);
}

TEST_F(DiscoveryServerLifecycleTest, StopWithoutStart)
{
    server = std::make_unique<UdpDiscoveryServer>(*io_context, test_port);

    ASSERT_NO_THROW(server->stop());
}

TEST_F(DiscoveryServerLifecycleTest, MultipleStartCalls)
{
    server = std::make_unique<UdpDiscoveryServer>(*io_context, test_port);

    bool first_start = server->async_start();
    std::this_thread::sleep_for(wait_for_async);

    // Second start should be ignored
    bool second_start = server->async_start();

    // Third start should also be ignored
    bool third_start = server->async_start();

    EXPECT_TRUE(first_start);
    EXPECT_FALSE(second_start);
    EXPECT_FALSE(third_start);

    server->stop();
}

TEST_F(DiscoveryServerLifecycleTest, MultipleAsyncStopCalls)
{
    std::atomic<int> stop_callback_count {0};

    server = std::make_unique<UdpDiscoveryServer>(*io_context, test_port);

    server->async_start();
    std::this_thread::sleep_for(wait_for_async);

    bool first_stop  = server->async_stop([&stop_callback_count]() { stop_callback_count++; });
    bool second_stop = server->async_stop([&stop_callback_count]() { stop_callback_count++; });

    EXPECT_TRUE(first_stop);
    EXPECT_FALSE(second_stop);

    std::this_thread::sleep_for(wait_for_operation);

    EXPECT_EQ(stop_callback_count, 1);
}

TEST_F(DiscoveryServerLifecycleTest, StopThenStart)
{
    std::atomic<bool> stopped {false};

    server = std::make_unique<UdpDiscoveryServer>(*io_context, test_port);

    server->async_start();
    std::this_thread::sleep_for(wait_for_async);

    server->async_stop([&stopped]() { stopped = true; });

    auto start_time = std::chrono::steady_clock::now();
    while (!stopped && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2))
    {
        std::this_thread::sleep_for(wait_for_async);
    }

    ASSERT_TRUE(stopped);

    // Starting after stop should be ignored while stopping
    bool start_result = server->async_start();
    std::this_thread::sleep_for(wait_for_operation);

    EXPECT_FALSE(start_result);
}

TEST_F(DiscoveryServerLifecycleTest, DestructionAfterStart)
{
    server = std::make_unique<UdpDiscoveryServer>(*io_context, test_port);

    server->async_start();
    std::this_thread::sleep_for(wait_for_async);

    ASSERT_NO_THROW(server.reset());
}

TEST_F(DiscoveryServerLifecycleTest, DestructionWithoutStart)
{
    server = std::make_unique<UdpDiscoveryServer>(*io_context, test_port);

    ASSERT_NO_THROW(server.reset());
}

TEST_F(DiscoveryServerLifecycleTest, RapidStartStopCycles)
{
    server = std::make_unique<UdpDiscoveryServer>(*io_context, test_port);

    for (int i = 0; i < 5; ++i)
    {
        ASSERT_TRUE(server->async_start());
        std::this_thread::sleep_for(wait_for_async);
        server->stop();
        server->reset();
    }
}

TEST_F(DiscoveryServerLifecycleTest, AddDiscoverableBeforeStart)
{
    server = std::make_unique<UdpDiscoveryServer>(*io_context, test_port);

    Discoverable discoverable(test_service, [](const auto&) { return std::string("test_reply"); });
    ASSERT_NO_THROW(server->add_discoverable(discoverable));

    server->async_start();
    std::this_thread::sleep_for(wait_for_operation);

    server->stop();
}

TEST_F(DiscoveryServerLifecycleTest, AddDiscoverableAfterStart)
{
    server = std::make_unique<UdpDiscoveryServer>(*io_context, test_port);

    server->async_start();
    std::this_thread::sleep_for(wait_for_async);

    Discoverable discoverable(test_service, [](const auto&) { return std::string("test_reply"); });
    ASSERT_NO_THROW(server->add_discoverable(discoverable));

    server->stop();
}

TEST_F(DiscoveryServerLifecycleTest, AddMultipleDiscoverables)
{
    server = std::make_unique<UdpDiscoveryServer>(*io_context, test_port);

    Discoverable discoverable1("service1", [](const auto&) { return std::string("reply1"); });
    Discoverable discoverable2("service2", [](const auto&) { return std::string("reply2"); });
    Discoverable discoverable3("service3", [](const auto&) { return std::string("reply3"); });

    ASSERT_NO_THROW(server->add_discoverable(discoverable1));
    ASSERT_NO_THROW(server->add_discoverable(discoverable2));
    ASSERT_NO_THROW(server->add_discoverable(discoverable3));

    server->async_start();
    std::this_thread::sleep_for(wait_for_operation);

    server->stop();
}

TEST_F(DiscoveryServerLifecycleTest, ResetAfterStop)
{
    server = std::make_unique<UdpDiscoveryServer>(*io_context, test_port);

    server->async_start();
    std::this_thread::sleep_for(wait_for_async);

    server->stop();

    ASSERT_NO_THROW(server->reset());

    // After reset, should be able to start again
    bool start_result = server->async_start();
    EXPECT_TRUE(start_result);

    server->stop();
}

TEST_F(DiscoveryServerLifecycleTest, AsyncStopWithoutCallback)
{
    server = std::make_unique<UdpDiscoveryServer>(*io_context, test_port);

    server->async_start();
    std::this_thread::sleep_for(wait_for_async);

    // async_stop with nullptr callback should not crash
    bool stop_initiated = server->async_stop(nullptr);

    EXPECT_TRUE(stop_initiated);

    std::this_thread::sleep_for(wait_for_operation);
}
