#include "eestv/net/connection/tcp_server.hpp"
#include <boost/asio.hpp>
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

using namespace eestv;

class TcpServerLifeCycleTest : public ::testing::Test
{
protected:
    static constexpr std::chrono::milliseconds startup_delay {100};

    void SetUp() override
    {
        io_context = std::make_unique<boost::asio::io_context>();
        // work_guard = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(io_context->get_executor());

        // Start io_context in background thread
        io_thread = std::thread(
            [this]()
            {
                io_context->run();
                std::cout << "Io context stopped \n";
            });
    }

    void TearDown() override
    {
        std::cout << "[TearDown] Starting cleanup..." << std::endl;

        // Stop io_context and wait for thread to finish
        // test::IoContextDebugger::print_state(*io_context, "Before work_guard reset");

        // work_guard.reset();

        // test::IoContextDebugger::print_state(*io_context, "After work_guard reset");

        // Wait for io_context to become idle (with timeout)
        // std::cout << "[TearDown] Waiting for io_context to become idle..." << std::endl;
        // bool idle = test::IoContextDebugger::wait_for_idle(*io_context, std::chrono::seconds(5));

        // if (!idle)
        // {
        //     std::cout << "[TearDown] WARNING: io_context did not become idle within timeout!" << std::endl;
        //     test::IoContextDebugger::force_stop_with_diagnostics(*io_context);
        // }
        // else
        // {
        //     std::cout << "[TearDown] io_context became idle naturally" << std::endl;
        // }

        // io_context->stop();

        if (io_thread.joinable())
        {
            io_thread.join();
        }
    }

    std::unique_ptr<boost::asio::io_context> io_context;
    // std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard;
    std::thread io_thread;
};

// Test basic server creation and destruction
TEST_F(TcpServerLifeCycleTest, CreateStartAndDestroyServer)
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
    server->async_stop();

    // Give the server a short time to process the stop; poll is_running() a few times
    for (int i = 0; i < 10 && server->is_running(); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_FALSE(server->is_running());

    // Destroy the server (unique_ptr will automatically clean up)
    server.reset();

    // Test passes if we reach here without crashes or exceptions
    SUCCEED();
}

// Test server creation with specific endpoint
TEST_F(TcpServerLifeCycleTest, CreateStartAndDestroyServerViaDestructor)
{
    // Create server on any available port
    auto server = std::make_unique<TcpServer<>>(*io_context, 0);

    // Verify server is created but not running
    EXPECT_FALSE(server->is_running());

    std::atomic<bool> stopped {false};

    // Start the server
    server->async_start();

    // Give server time to start
    std::this_thread::sleep_for(startup_delay);

    // Verify server is now running
    EXPECT_TRUE(server->is_running());

    // Get the port the server is listening on
    unsigned short port = server->port();
    EXPECT_GT(port, 0);

    server->set_stopped_callback(
        [&stopped]()
        {
            std::cout << "Server has stopped callback invoked\n";
            stopped = true;
        });

    server.reset();

    EXPECT_TRUE(stopped.load());

    // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    // Destroy the server (unique_ptr will automatically clean up)

    // Test passes if we reach here without crashes or exceptions
    SUCCEED();
}
