#include "eestv/logging/eestv_logging.hpp"
#include "eestv/net/discovery/udp_discovery_client.hpp"
#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>

namespace
{
const int test_port            = 54323;
const std::string test_service = "test_service";
const auto short_timeout       = std::chrono::milliseconds(100);
const auto medium_timeout      = std::chrono::milliseconds(500);
const auto wait_for_async      = std::chrono::milliseconds(50);
const auto wait_for_operation  = std::chrono::milliseconds(200);
} // namespace

using eestv::UdpDiscoveryClient;

/**
 * Lifecycle tests for UdpDiscoveryClient - tests creation, start, stop, and destruction
 */
class DiscoveryClientLifecycleTest : public ::testing::Test
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
        if (client)
        {
            client->stop();
            client.reset();
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
    std::unique_ptr<UdpDiscoveryClient> client;
    std::thread io_thread;
};

TEST_F(DiscoveryClientLifecycleTest, Destruct)
{
    ASSERT_NO_THROW({
        client = std::make_unique<UdpDiscoveryClient>(*io_context, test_service, short_timeout, test_port,
                                                      [](const std::string&, const boost::asio::ip::udp::endpoint&) { return true; });
    });

    EXPECT_NE(client, nullptr);
}

TEST_F(DiscoveryClientLifecycleTest, StartAndDestruct)
{
    ASSERT_NO_THROW({
        client = std::make_unique<UdpDiscoveryClient>(*io_context, test_service, short_timeout, test_port,
                                                      [](const std::string&, const boost::asio::ip::udp::endpoint&) { return true; });
    });

    client->async_start();

    std::this_thread::sleep_for(wait_for_operation);

    EXPECT_NE(client, nullptr);
}

TEST_F(DiscoveryClientLifecycleTest, StartAndDestructImmediately)
{
    std::atomic<int> callback_count {0};

    client = std::make_unique<UdpDiscoveryClient>(*io_context, test_service, short_timeout, test_port,
                                                  [&callback_count](const std::string&, const boost::asio::ip::udp::endpoint&)
                                                  {
                                                      callback_count++;
                                                      return true;
                                                  });

    ASSERT_NO_THROW(client->async_start());
}

TEST_F(DiscoveryClientLifecycleTest, StartStopAndDestruct)
{
    ASSERT_NO_THROW({
        client = std::make_unique<UdpDiscoveryClient>(*io_context, test_service, short_timeout, test_port,
                                                      [](const std::string&, const boost::asio::ip::udp::endpoint&) { return true; });
    });

    client->async_start();

    std::this_thread::sleep_for(wait_for_operation);

    client->stop();

    EXPECT_NE(client, nullptr);
}

TEST_F(DiscoveryClientLifecycleTest, StartStopImmediatelyAndDestruct)
{
    ASSERT_NO_THROW({
        client = std::make_unique<UdpDiscoveryClient>(*io_context, test_service, short_timeout, test_port,
                                                      [](const std::string&, const boost::asio::ip::udp::endpoint&) { return true; });
    });

    client->async_start();
    client->stop();

    EXPECT_NE(client, nullptr);
}

TEST_F(DiscoveryClientLifecycleTest, StartAndAsyncStop)
{
    std::atomic<bool> stopped {false};

    client = std::make_unique<UdpDiscoveryClient>(*io_context, test_service, short_timeout, test_port,
                                                  [](const std::string&, const boost::asio::ip::udp::endpoint&) { return true; });

    client->async_start();
    std::this_thread::sleep_for(wait_for_async);

    bool stop_initiated = client->async_stop([&stopped]() { stopped = true; });

    EXPECT_TRUE(stop_initiated);

    auto start_time = std::chrono::steady_clock::now();
    while (!stopped && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2))
    {
        std::this_thread::sleep_for(wait_for_async);
    }

    EXPECT_TRUE(stopped);
}

TEST_F(DiscoveryClientLifecycleTest, StopWithoutStart)
{
    client = std::make_unique<UdpDiscoveryClient>(*io_context, test_service, short_timeout, test_port,
                                                  [](const std::string&, const boost::asio::ip::udp::endpoint&) { return true; });

    ASSERT_NO_THROW(client->stop());
}

TEST_F(DiscoveryClientLifecycleTest, MultipleStartCalls)
{
    client = std::make_unique<UdpDiscoveryClient>(*io_context, test_service, short_timeout, test_port,
                                                  [](const std::string&, const boost::asio::ip::udp::endpoint&) { return true; });

    client->async_start();
    std::this_thread::sleep_for(wait_for_async);

    // Second start should be ignored
    ASSERT_NO_THROW(client->async_start());

    // Third start should also be ignored
    ASSERT_NO_THROW(client->async_start());

    client->stop();
}

TEST_F(DiscoveryClientLifecycleTest, MultipleAsyncStopCalls)
{
    std::atomic<int> stop_callback_count {0};

    client = std::make_unique<UdpDiscoveryClient>(*io_context, test_service, short_timeout, test_port,
                                                  [](const std::string&, const boost::asio::ip::udp::endpoint&) { return true; });

    client->async_start();
    std::this_thread::sleep_for(wait_for_async);

    bool first_stop  = client->async_stop([&stop_callback_count]() { stop_callback_count++; });
    bool second_stop = client->async_stop([&stop_callback_count]() { stop_callback_count++; });

    EXPECT_TRUE(first_stop);
    EXPECT_FALSE(second_stop);

    std::this_thread::sleep_for(wait_for_operation);

    EXPECT_EQ(stop_callback_count, 1);
}

TEST_F(DiscoveryClientLifecycleTest, StopThenStart)
{
    std::atomic<bool> stopped {false};

    client = std::make_unique<UdpDiscoveryClient>(*io_context, test_service, short_timeout, test_port,
                                                  [](const std::string&, const boost::asio::ip::udp::endpoint&) { return true; });

    client->async_start();
    std::this_thread::sleep_for(wait_for_async);

    client->async_stop([&stopped]() { stopped = true; });

    auto start_time = std::chrono::steady_clock::now();
    while (!stopped && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2))
    {
        std::this_thread::sleep_for(wait_for_async);
    }

    ASSERT_TRUE(stopped);

    // Starting after stop should be ignored while stopping
    client->async_start();
    std::this_thread::sleep_for(wait_for_operation);
}

TEST_F(DiscoveryClientLifecycleTest, DestructionAfterStart)
{
    client = std::make_unique<UdpDiscoveryClient>(*io_context, test_service, short_timeout, test_port,
                                                  [](const std::string&, const boost::asio::ip::udp::endpoint&) { return true; });

    client->async_start();
    std::this_thread::sleep_for(wait_for_async);

    ASSERT_NO_THROW(client.reset());
}

TEST_F(DiscoveryClientLifecycleTest, DestructionWithoutStart)
{
    client = std::make_unique<UdpDiscoveryClient>(*io_context, test_service, short_timeout, test_port,
                                                  [](const std::string&, const boost::asio::ip::udp::endpoint&) { return true; });

    ASSERT_NO_THROW(client.reset());
}

TEST_F(DiscoveryClientLifecycleTest, RapidStartStopCycles)
{
    client = std::make_unique<UdpDiscoveryClient>(*io_context, test_service, short_timeout, test_port,
                                                  [](const std::string&, const boost::asio::ip::udp::endpoint&) { return true; });

    for (int i = 0; i < 5; ++i)
    {
        ASSERT_TRUE(client->async_start());
        std::this_thread::sleep_for(wait_for_async);
        client->stop();
        client->reset();
    }
}
