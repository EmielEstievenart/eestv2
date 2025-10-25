#include "eestv/net/connection/tcp_client.hpp"
#include "eestv/net/connection/tcp_server.hpp"
#include "eestv/logging/eestv_logging.hpp"
#include <boost/asio.hpp>
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

using namespace eestv;

class TcpClientLifeCycleTests : public ::testing::Test
{
protected:
    static constexpr std::chrono::milliseconds startup_delay {100};
    static constexpr std::chrono::milliseconds short_delay {10};
    static constexpr std::chrono::milliseconds medium_delay {50};
    static constexpr std::chrono::milliseconds long_delay {100};

    void SetUp() override
    {
        EESTV_SET_LOG_LEVEL(Trace);
        io_context = std::make_unique<boost::asio::io_context>();
        work_guard = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(io_context->get_executor());
        io_thread  = std::thread(
            [this]()
            {
                io_context->run();
                EESTV_LOG_INFO("IO context stopped");
            });
        server = std::make_unique<TcpServer<>>(*io_context, 0);
        server->async_start();
        std::this_thread::sleep_for(startup_delay);
        port = server->port();
    }

    void TearDown() override
    {
        EESTV_LOG_INFO("[TearDown] Starting cleanup...");

        server.reset();
        work_guard.reset();
        io_context->stop();
        if (io_thread.joinable())
        {
            io_thread.join();
        }

        EESTV_LOG_INFO("[TearDown] Cleanup complete");
    }

    std::unique_ptr<boost::asio::io_context> io_context;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard;
    std::thread io_thread;
    std::unique_ptr<TcpServer<>> server;
    unsigned short port;
};

// Test: Basic TcpClient construction and destruction
TEST_F(TcpClientLifeCycleTests, BasicConstructionAndDestruction)
{
    EESTV_LOG_INFO("[TEST] Starting BasicConstructionAndDestruction");

    ASSERT_NO_THROW({ auto client = std::make_unique<TcpClient>(*io_context); });

    EESTV_LOG_INFO("[TEST] BasicConstructionAndDestruction completed");
    SUCCEED();
}

// Test: Connect to server and destruct
TEST_F(TcpClientLifeCycleTests, ConnectAndDestruct)
{
    EESTV_LOG_INFO("[TEST] Starting ConnectAndDestruct");

    EESTV_LOG_INFO("[SERVER] Listening on port " << port);

    std::unique_ptr<TcpConnection<>> connection_from_server;

    std::atomic<bool> connection_received {false};
    server->set_connection_callback(
        [&](std::unique_ptr<TcpConnection<>> new_connection)
        {
            connection_from_server = std::move(new_connection);
            EESTV_LOG_INFO("[SERVER] Connection received");
            connection_received = true;
        });

    // Create client
    auto client = std::make_unique<TcpClient>(*io_context);

    std::atomic<bool> connected {false};
    std::atomic<bool> error_occurred {false};
    std::unique_ptr<boost::asio::ip::tcp::socket> received_socket;

    bool connect_initiated = client->async_connect("127.0.0.1", port,
                                                   [&](boost::asio::ip::tcp::socket&& socket, const boost::system::error_code& error)
                                                   {
                                                       if (error)
                                                       {
                                                           EESTV_LOG_ERROR("[CLIENT] Error: " << error.message());
                                                           error_occurred = true;
                                                       }
                                                       else
                                                       {
                                                           EESTV_LOG_INFO("[CLIENT] Connected");
                                                           received_socket =
                                                               std::make_unique<boost::asio::ip::tcp::socket>(std::move(socket));
                                                           connected = true;
                                                       }
                                                   });

    ASSERT_TRUE(connect_initiated);
    EESTV_LOG_INFO("[CLIENT] Connect initiated");

    // Wait for connection
    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(200);
    while (!connected && !error_occurred && std::chrono::steady_clock::now() < timeout)
    {
        std::this_thread::sleep_for(short_delay);
    }

    EESTV_LOG_INFO("[CLIENT] After wait: connected=" << connected << ", error=" << error_occurred);

    EXPECT_TRUE(connected);
    EXPECT_FALSE(error_occurred);
    EXPECT_TRUE(connection_received);

    // Print out the flag states
    EESTV_LOG_DEBUG("[CLIENT] " << client->to_string());

    // Destruct client
    ASSERT_NO_THROW(client.reset());

    EESTV_LOG_INFO("[TEST] ConnectAndDestruct completed");
    SUCCEED();
}

// Test: Start connecting and immediately destruct
TEST_F(TcpClientLifeCycleTests, ConnectAndImmediateDestruct)
{
    EESTV_LOG_INFO("[TEST] Starting ConnectAndImmediateDestruct");

    // Create client
    auto client = std::make_unique<TcpClient>(*io_context);

    bool connect_initiated = client->async_connect(
        "127.0.0.1", port, [&](boost::asio::ip::tcp::socket&& /*socket*/, const boost::system::error_code& /*error*/) { });

    ASSERT_TRUE(connect_initiated);

    // Immediately destruct without waiting for connection
    ASSERT_NO_THROW(client.reset());

    EESTV_LOG_INFO("[TEST] ConnectAndImmediateDestruct completed");
    SUCCEED();
}

// Test: Async stop with callback
TEST_F(TcpClientLifeCycleTests, AsyncStopWithCallback)
{
    EESTV_LOG_INFO("[TEST] Starting AsyncStopWithCallback");

    auto client = std::make_unique<TcpClient>(*io_context);

    bool connect_initiated = client->async_connect(
        "127.0.0.1", port, [&](boost::asio::ip::tcp::socket&& /*socket*/, const boost::system::error_code& /*error*/) { });

    ASSERT_TRUE(connect_initiated);

    // Give it a moment to start connecting
    std::this_thread::sleep_for(medium_delay);

    std::atomic<bool> stopped {false};
    bool stop_initiated = client->async_stop([&stopped]() { stopped = true; });

    EXPECT_TRUE(stop_initiated);

    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!stopped && std::chrono::steady_clock::now() < timeout)
    {
        std::this_thread::sleep_for(short_delay);
    }

    EXPECT_TRUE(stopped);

    EESTV_LOG_INFO("[TEST] AsyncStopWithCallback completed");
    SUCCEED();
}

// Test: Synchronous stop
TEST_F(TcpClientLifeCycleTests, SyncStop)
{
    EESTV_LOG_INFO("[TEST] Starting SyncStop");

    auto client = std::make_unique<TcpClient>(*io_context);

    bool connect_initiated = client->async_connect(
        "127.0.0.1", port, [&](boost::asio::ip::tcp::socket&& /*socket*/, const boost::system::error_code& /*error*/) { });

    ASSERT_TRUE(connect_initiated);

    // Give it a moment to start connecting
    std::this_thread::sleep_for(medium_delay);

    // Synchronous stop should block until stopped
    ASSERT_NO_THROW(client->stop());

    EESTV_LOG_INFO("[TEST] SyncStop completed");
    SUCCEED();
}

// Test: Destruction without connecting
TEST_F(TcpClientLifeCycleTests, DestructionWithoutConnect)
{
    EESTV_LOG_INFO("[TEST] Starting DestructionWithoutConnect");

    auto client = std::make_unique<TcpClient>(*io_context);

    // Destruct without calling async_connect
    ASSERT_NO_THROW(client.reset());

    EESTV_LOG_INFO("[TEST] DestructionWithoutConnect completed");
    SUCCEED();
}

// Test: Multiple connect attempts
TEST_F(TcpClientLifeCycleTests, MultipleConnectAttempts)
{
    EESTV_LOG_INFO("[TEST] Starting MultipleConnectAttempts");

    auto client = std::make_unique<TcpClient>(*io_context);

    std::atomic<bool> first_completed {false};

    bool first_connect =
        client->async_connect("127.0.0.1", port, [&](boost::asio::ip::tcp::socket&& /*socket*/, const boost::system::error_code& /*error*/)
                              { first_completed = true; });

    ASSERT_TRUE(first_connect);

    // Second connect attempt should be rejected while first is in progress
    bool second_connect = client->async_connect(
        "127.0.0.1", port, [&](boost::asio::ip::tcp::socket&& /*socket*/, const boost::system::error_code& /*error*/) { });

    EXPECT_FALSE(second_connect);

    // Wait for first connection to complete
    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!first_completed && std::chrono::steady_clock::now() < timeout)
    {
        std::this_thread::sleep_for(short_delay);
    }

    EESTV_LOG_INFO("[TEST] MultipleConnectAttempts completed");
    SUCCEED();
}

// Test: Multiple async_stop calls
TEST_F(TcpClientLifeCycleTests, MultipleAsyncStopCalls)
{
    EESTV_LOG_INFO("[TEST] Starting MultipleAsyncStopCalls");

    auto client = std::make_unique<TcpClient>(*io_context);

    std::atomic<int> stop_callback_count {0};

    bool connect_initiated = client->async_connect(
        "127.0.0.1", port, [&](boost::asio::ip::tcp::socket&& /*socket*/, const boost::system::error_code& /*error*/) { });

    ASSERT_TRUE(connect_initiated);

    std::this_thread::sleep_for(medium_delay);

    bool first_stop  = client->async_stop([&stop_callback_count]() { stop_callback_count++; });
    bool second_stop = client->async_stop([&stop_callback_count]() { stop_callback_count++; });

    EXPECT_TRUE(first_stop);
    EXPECT_FALSE(second_stop);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(stop_callback_count, 1);

    EESTV_LOG_INFO("[TEST] MultipleAsyncStopCalls completed");
    SUCCEED();
}

// Test: Stop without connect
TEST_F(TcpClientLifeCycleTests, StopWithoutConnect)
{
    EESTV_LOG_INFO("[TEST] Starting StopWithoutConnect");

    auto client = std::make_unique<TcpClient>(*io_context);

    bool stop_result = client->async_stop(nullptr);

    EXPECT_TRUE(stop_result);

    std::this_thread::sleep_for(medium_delay);

    EESTV_LOG_INFO("[TEST] StopWithoutConnect completed");
    SUCCEED();
}

// Test: Connect to invalid host
TEST_F(TcpClientLifeCycleTests, ConnectToInvalidHost)
{
    EESTV_LOG_INFO("[TEST] Starting ConnectToInvalidHost");

    auto client = std::make_unique<TcpClient>(*io_context);

    std::atomic<bool> completed {false};
    std::atomic<bool> had_error {false};

    bool connect_initiated = client->async_connect("invalid.host.that.does.not.exist.local", 12345,
                                                   [&](boost::asio::ip::tcp::socket&& /*socket*/, const boost::system::error_code& error)
                                                   {
                                                       completed = true;
                                                       if (error)
                                                       {
                                                           had_error = true;
                                                       }
                                                   });

    ASSERT_TRUE(connect_initiated);

    // Wait for completion
    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!completed && std::chrono::steady_clock::now() < timeout)
    {
        std::this_thread::sleep_for(short_delay);
    }

    EXPECT_TRUE(completed);
    EXPECT_TRUE(had_error);

    EESTV_LOG_INFO("[TEST] ConnectToInvalidHost completed");
    SUCCEED();
}

// Test: Connect to unreachable port
TEST_F(TcpClientLifeCycleTests, ConnectToUnreachablePort)
{
    EESTV_LOG_INFO("[TEST] Starting ConnectToUnreachablePort");

    auto client = std::make_unique<TcpClient>(*io_context);

    std::atomic<bool> completed {false};
    std::atomic<bool> had_error {false};

    // Connect to localhost on a port that's likely not listening
    bool connect_initiated = client->async_connect("127.0.0.1", 9, // discard port, typically not used
                                                   [&](boost::asio::ip::tcp::socket&& /*socket*/, const boost::system::error_code& error)
                                                   {
                                                       completed = true;
                                                       if (error)
                                                       {
                                                           had_error = true;
                                                       }
                                                   });

    ASSERT_TRUE(connect_initiated);

    // Wait for completion
    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!completed && std::chrono::steady_clock::now() < timeout)
    {
        std::this_thread::sleep_for(short_delay);
    }

    EXPECT_TRUE(completed);
    EXPECT_TRUE(had_error);

    EESTV_LOG_INFO("[TEST] ConnectToUnreachablePort completed");
    SUCCEED();
}

// Test: Reset after successful connection
TEST_F(TcpClientLifeCycleTests, ResetAfterConnection)
{
    EESTV_LOG_INFO("[TEST] Starting ResetAfterConnection");

    auto client = std::make_unique<TcpClient>(*io_context);

    std::atomic<bool> first_connected {false};

    bool connect_initiated = client->async_connect("127.0.0.1", port,
                                                   [&](boost::asio::ip::tcp::socket&& /*socket*/, const boost::system::error_code& error)
                                                   {
                                                       if (!error)
                                                       {
                                                           first_connected = true;
                                                       }
                                                   });

    ASSERT_TRUE(connect_initiated);

    // Wait for connection
    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!first_connected && std::chrono::steady_clock::now() < timeout)
    {
        std::this_thread::sleep_for(short_delay);
    }

    ASSERT_TRUE(first_connected);

    // Print out the flag states before reset
    EESTV_LOG_DEBUG("[CLIENT] Before reset: " << client->to_string());

    // Reset and try to connect again
    ASSERT_NO_THROW(client->reset());

    std::atomic<bool> second_connected {false};

    bool second_connect = client->async_connect("127.0.0.1", port,
                                                [&](boost::asio::ip::tcp::socket&& /*socket*/, const boost::system::error_code& error)
                                                {
                                                    if (!error)
                                                    {
                                                        second_connected = true;
                                                    }
                                                });

    EXPECT_TRUE(second_connect);

    // Wait for second connection
    timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!second_connected && std::chrono::steady_clock::now() < timeout)
    {
        std::this_thread::sleep_for(short_delay);
    }

    EXPECT_TRUE(second_connected);

    EESTV_LOG_INFO("[TEST] ResetAfterConnection completed");
    SUCCEED();
}

// Test: Reset after stop
TEST_F(TcpClientLifeCycleTests, ResetAfterStop)
{
    EESTV_LOG_INFO("[TEST] Starting ResetAfterStop");

    auto client = std::make_unique<TcpClient>(*io_context);

    bool connect_initiated = client->async_connect(
        "127.0.0.1", port, [&](boost::asio::ip::tcp::socket&& /*socket*/, const boost::system::error_code& /*error*/) { });

    ASSERT_TRUE(connect_initiated);

    std::this_thread::sleep_for(medium_delay);

    std::atomic<bool> stopped {false};
    bool stop_initiated = client->async_stop([&stopped]() { stopped = true; });

    ASSERT_TRUE(stop_initiated);

    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!stopped && std::chrono::steady_clock::now() < timeout)
    {
        std::this_thread::sleep_for(short_delay);
    }

    ASSERT_TRUE(stopped);

    // Print out the flag states before reset
    EESTV_LOG_DEBUG("[CLIENT] Before reset: " << client->to_string());

    // Reset after stop
    ASSERT_NO_THROW(client->reset());

    // Try to connect again
    std::atomic<bool> connected {false};

    bool second_connect = client->async_connect("127.0.0.1", port,
                                                [&](boost::asio::ip::tcp::socket&& /*socket*/, const boost::system::error_code& error)
                                                {
                                                    if (!error)
                                                    {
                                                        connected = true;
                                                    }
                                                });

    EXPECT_TRUE(second_connect);

    timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!connected && std::chrono::steady_clock::now() < timeout)
    {
        std::this_thread::sleep_for(short_delay);
    }

    EXPECT_TRUE(connected);

    EESTV_LOG_INFO("[TEST] ResetAfterStop completed");
    SUCCEED();
}

// Test: Connect attempt while stopping
TEST_F(TcpClientLifeCycleTests, ConnectWhileStopping)
{
    EESTV_LOG_INFO("[TEST] Starting ConnectWhileStopping");

    auto client = std::make_unique<TcpClient>(*io_context);

    bool connect_initiated = client->async_connect(
        "127.0.0.1", port, [&](boost::asio::ip::tcp::socket&& /*socket*/, const boost::system::error_code& /*error*/) { });

    ASSERT_TRUE(connect_initiated);

    std::this_thread::sleep_for(medium_delay);

    std::atomic<bool> stopped {false};
    bool stop_initiated = client->async_stop([&stopped]() { stopped = true; });

    ASSERT_TRUE(stop_initiated);

    // Try to connect while stopping - should be rejected
    bool second_connect = client->async_connect(
        "127.0.0.1", port, [&](boost::asio::ip::tcp::socket&& /*socket*/, const boost::system::error_code& /*error*/) { });

    EXPECT_FALSE(second_connect);

    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!stopped && std::chrono::steady_clock::now() < timeout)
    {
        std::this_thread::sleep_for(short_delay);
    }

    EXPECT_TRUE(stopped);

    EESTV_LOG_INFO("[TEST] ConnectWhileStopping completed");
    SUCCEED();
}

// Test: Async stop without callback
TEST_F(TcpClientLifeCycleTests, AsyncStopWithoutCallback)
{
    EESTV_LOG_INFO("[TEST] Starting AsyncStopWithoutCallback");

    auto client = std::make_unique<TcpClient>(*io_context);

    bool connect_initiated = client->async_connect(
        "127.0.0.1", port, [&](boost::asio::ip::tcp::socket&& /*socket*/, const boost::system::error_code& /*error*/) { });

    ASSERT_TRUE(connect_initiated);

    std::this_thread::sleep_for(medium_delay);

    // async_stop with nullptr callback should not crash
    bool stop_initiated = client->async_stop(nullptr);

    EXPECT_TRUE(stop_initiated);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EESTV_LOG_INFO("[TEST] AsyncStopWithoutCallback completed");
    SUCCEED();
}

// Test: Destruction during connection attempt
TEST_F(TcpClientLifeCycleTests, DestructionDuringConnection)
{
    EESTV_LOG_INFO("[TEST] Starting DestructionDuringConnection");

    auto client = std::make_unique<TcpClient>(*io_context);

    bool connect_initiated = client->async_connect(
        "127.0.0.1", port, [&](boost::asio::ip::tcp::socket&& /*socket*/, const boost::system::error_code& /*error*/) { });

    ASSERT_TRUE(connect_initiated);

    // Give it a short time to start connecting but not complete
    std::this_thread::sleep_for(short_delay);

    // Destruct during connection attempt
    ASSERT_NO_THROW(client.reset());

    EESTV_LOG_INFO("[TEST] DestructionDuringConnection completed");
    SUCCEED();
}

// Test: Successful connection and socket handoff
TEST_F(TcpClientLifeCycleTests, SuccessfulConnectionSocketHandoff)
{
    EESTV_LOG_INFO("[TEST] Starting SuccessfulConnectionSocketHandoff");

    auto client = std::make_unique<TcpClient>(*io_context);

    std::atomic<bool> connected {false};
    std::unique_ptr<boost::asio::ip::tcp::socket> received_socket;

    bool connect_initiated = client->async_connect("127.0.0.1", port,
                                                   [&](boost::asio::ip::tcp::socket&& socket, const boost::system::error_code& error)
                                                   {
                                                       if (!error)
                                                       {
                                                           received_socket =
                                                               std::make_unique<boost::asio::ip::tcp::socket>(std::move(socket));
                                                           connected = true;
                                                       }
                                                   });

    ASSERT_TRUE(connect_initiated);

    // Wait for connection
    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!connected && std::chrono::steady_clock::now() < timeout)
    {
        std::this_thread::sleep_for(short_delay);
    }

    ASSERT_TRUE(connected);
    ASSERT_NE(received_socket, nullptr);
    EXPECT_TRUE(received_socket->is_open());

    // Verify we can use the socket
    EXPECT_NO_THROW({
        auto remote_endpoint = received_socket->remote_endpoint();
        EXPECT_EQ(remote_endpoint.port(), port);
    });

    EESTV_LOG_INFO("[TEST] SuccessfulConnectionSocketHandoff completed");
    SUCCEED();
}
