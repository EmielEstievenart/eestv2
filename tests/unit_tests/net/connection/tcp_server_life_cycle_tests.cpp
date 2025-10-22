#include "eestv/net/connection/tcp_server.hpp"
#include <boost/asio.hpp>
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

using namespace eestv;

class TcpServerLifeCycleTests : public ::testing::Test
{
protected:
    static constexpr std::chrono::milliseconds startup_delay {100};

    void SetUp() override
    {
        io_context = std::make_unique<boost::asio::io_context>();
        // Keep io_context alive until explicitly stopped
        work_guard = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(io_context->get_executor());

        // Start io_context in background thread
        io_thread = std::thread(
            [this]()
            {
                // auto work_guard = boost::asio::make_work_guard(io_context);

                io_context->run();
                std::cout << "Io context stopped \n";
            });
    }

    void TearDown() override
    {
        std::cout << "[TearDown] Starting cleanup..." << std::endl;

        work_guard.reset();

        io_context->stop();

        if (io_thread.joinable())
        {
            io_thread.join();
        }

        // io_context->restart();

        std::cout << "[TearDown] Cleanup complete" << std::endl;
    }

    std::unique_ptr<boost::asio::io_context> io_context;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard;
    std::thread io_thread;
};

// Test basic server creation and destruction
TEST_F(TcpServerLifeCycleTests, CreateStartAndDestroyServer)
{
    // Create server on any available port
    auto server = std::make_unique<TcpServer<>>(*io_context, 0);

    // Verify server is created but not running
    EXPECT_FALSE(server->is_running());

    // Start the server
    server->async_start();

    // Give server time to start
    std::this_thread::sleep_for(startup_delay);

    // Verify server is now running
    EXPECT_TRUE(server->is_running());

    // Get the port the server is listening on
    unsigned short port = server->port();
    EXPECT_GT(port, 0);

    // Stop the server
    std::atomic<bool> stopped {false};
    server->async_stop(
        [&stopped]()
        {
            std::cout << "Server has stopped callback invoked\n";
            stopped = true;
        });

    // Give the server a short time to process the stop
    for (int i = 0; i < 10 && !stopped.load(); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(stopped.load());
    EXPECT_FALSE(server->is_running());

    // Destroy the server (unique_ptr will automatically clean up)
    server.reset();

    // Test passes if we reach here without crashes or exceptions
    SUCCEED();
}

// Test server creation with specific endpoint
TEST_F(TcpServerLifeCycleTests, CreateStartAndDestroyServerViaDestructor)
{
    auto server = std::make_unique<TcpServer<>>(*io_context, 0);

    EXPECT_FALSE(server->is_running());

    std::atomic<bool> stopped {false};

    server->async_start();

    std::this_thread::sleep_for(startup_delay);

    EXPECT_TRUE(server->is_running());

    unsigned short port = server->port();
    EXPECT_GT(port, 0);

    server->async_stop(
        [&stopped]()
        {
            std::cout << "Server has stopped callback invoked\n";
            stopped = true;
        });

    // Wait for the stopped callback to be invoked
    for (int i = 0; i < 20 && !stopped.load(); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(stopped.load());

    SUCCEED();
}

TEST_F(TcpServerLifeCycleTests, Destruct)
{
    ASSERT_NO_THROW({ auto server = std::make_unique<TcpServer<>>(*io_context, 0); });
}

TEST_F(TcpServerLifeCycleTests, StartAndDestructImmediately)
{
    auto server = std::make_unique<TcpServer<>>(*io_context, 0);

    ASSERT_NO_THROW(server->async_start());
}

TEST_F(TcpServerLifeCycleTests, MultipleStartCalls)
{
    auto server = std::make_unique<TcpServer<>>(*io_context, 0);

    bool first_start = server->async_start();
    std::this_thread::sleep_for(startup_delay);

    // Second start should be ignored
    bool second_start = server->async_start();

    // Third start should also be ignored
    bool third_start = server->async_start();

    EXPECT_TRUE(first_start);
    EXPECT_FALSE(second_start);
    EXPECT_FALSE(third_start);

    std::atomic<bool> stopped {false};
    server->async_stop([&stopped]() { stopped = true; });

    for (int i = 0; i < 10 && !stopped.load(); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

TEST_F(TcpServerLifeCycleTests, MultipleAsyncStopCalls)
{
    std::atomic<int> stop_callback_count {0};

    auto server = std::make_unique<TcpServer<>>(*io_context, 0);

    server->async_start();
    std::this_thread::sleep_for(startup_delay);

    bool first_stop  = server->async_stop([&stop_callback_count]() { stop_callback_count++; });
    bool second_stop = server->async_stop([&stop_callback_count]() { stop_callback_count++; });

    EXPECT_TRUE(first_stop);
    EXPECT_FALSE(second_stop);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(stop_callback_count, 1);
}

TEST_F(TcpServerLifeCycleTests, StopWithoutStart)
{
    auto server = std::make_unique<TcpServer<>>(*io_context, 0);

    bool stop_result = server->async_stop(nullptr);

    EXPECT_FALSE(stop_result);
}

TEST_F(TcpServerLifeCycleTests, StopThenStart)
{
    std::atomic<bool> stopped {false};

    auto server = std::make_unique<TcpServer<>>(*io_context, 0);

    server->async_start();
    std::this_thread::sleep_for(startup_delay);

    server->async_stop([&stopped]() { stopped = true; });

    auto start_time = std::chrono::steady_clock::now();
    while (!stopped && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ASSERT_TRUE(stopped);

    // Starting after stop should be ignored while stopping
    bool start_result = server->async_start();

    EXPECT_FALSE(start_result);
}

TEST_F(TcpServerLifeCycleTests, DestructionAfterStart)
{
    auto server = std::make_unique<TcpServer<>>(*io_context, 0);

    server->async_start();
    std::this_thread::sleep_for(startup_delay);

    ASSERT_NO_THROW(server.reset());
}

TEST_F(TcpServerLifeCycleTests, DestructionWithoutStart)
{
    auto server = std::make_unique<TcpServer<>>(*io_context, 0);

    ASSERT_NO_THROW(server.reset());
}

TEST_F(TcpServerLifeCycleTests, AsyncStopWithoutCallback)
{
    auto server = std::make_unique<TcpServer<>>(*io_context, 0);

    server->async_start();
    std::this_thread::sleep_for(startup_delay);

    // async_stop with nullptr callback should not crash
    bool stop_initiated = server->async_stop(nullptr);

    EXPECT_TRUE(stop_initiated);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_F(TcpServerLifeCycleTests, SetConnectionCallbackBeforeStart)
{
    auto server = std::make_unique<TcpServer<>>(*io_context, 0);

    std::atomic<int> connection_count {0};
    ASSERT_NO_THROW(server->set_connection_callback([&connection_count](auto connection) { connection_count++; }));

    server->async_start();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::atomic<bool> stopped {false};
    server->async_stop([&stopped]() { stopped = true; });

    for (int i = 0; i < 10 && !stopped.load(); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

TEST_F(TcpServerLifeCycleTests, SetConnectionCallbackAfterStart)
{
    auto server = std::make_unique<TcpServer<>>(*io_context, 0);

    server->async_start();
    std::this_thread::sleep_for(startup_delay);

    std::atomic<int> connection_count {0};
    ASSERT_NO_THROW(server->set_connection_callback([&connection_count](auto connection) { connection_count++; }));

    std::atomic<bool> stopped {false};
    server->async_stop([&stopped]() { stopped = true; });

    for (int i = 0; i < 10 && !stopped.load(); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
