#include "eestv/net/connection/tcp_connection.hpp"
#include "eestv/net/connection/tcp_server.hpp"
#include "io_context_debugger.hpp"
#include <boost/asio.hpp>
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
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

TEST_F(TcpServerLifeCycleTest, ServerConnectionShutdownTest)
{
    TcpServer<>* server = new TcpServer<>(*io_context, 0);

    std::mutex connection_mutex;
    std::condition_variable connection_cv;
    std::unique_ptr<TcpConnection<>> connection_from_server;
    bool connection_received = false;

    server->set_connection_callback(
        [&](std::unique_ptr<TcpConnection<>> connection)
        {
            std::cout << "[SERVER] New connection accepted\n";
            std::unique_lock<std::mutex> lock(connection_mutex);
            connection_from_server = std::move(connection);
            connection_received    = true;
            connection_cv.notify_one();
        });

    server->async_start();

    //Short sleep to allow server to start
    std::this_thread::sleep_for(startup_delay);

    unsigned short port = server->port();
    std::cout << "[SERVER] Listening on port " << port << "\n";

    boost::asio::ip::tcp::socket client_socket(*io_context);
    boost::system::error_code connect_error;

    std::atomic<bool> client_connected {false};

    client_socket.async_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port),
                                [&](const boost::system::error_code& error)
                                {
                                    connect_error    = error;
                                    client_connected = true;
                                    std::cout << "[CLIENT] Connection attempt completed\n";
                                });

    auto connect_timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!client_connected && std::chrono::steady_clock::now() < connect_timeout)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_TRUE(client_connected) << "Client failed to connect";
    ASSERT_FALSE(connect_error) << "Client connection error: " << connect_error.message();
    std::cout << "[CLIENT] Connected successfully\n";

    // Wait for server to accept the connection
    {
        std::unique_lock<std::mutex> lock(connection_mutex);
        bool received = connection_cv.wait_for(lock, std::chrono::seconds(2), [&] { return connection_received; });
        ASSERT_TRUE(received) << "Server did not accept connection within timeout";
    }

    ASSERT_NE(connection_from_server, nullptr) << "Connection from server is null";
    std::cout << "[TEST] Server connection established\n";

    ASSERT_TRUE(client_socket.is_open()) << "Client socket should be open";

    std::cout << "[CLIENT] Closing client socket\n";
    boost::system::error_code close_error;
    client_socket.close(close_error);
    ASSERT_FALSE(close_error) << "Client close error: " << close_error.message();

    std::cout << "[TEST] Destroying server connection (testing shutdown)...\n";
    auto destruction_start = std::chrono::steady_clock::now();

    connection_from_server.reset(); // This should trigger the destructor

    auto destruction_end      = std::chrono::steady_clock::now();
    auto destruction_duration = std::chrono::duration_cast<std::chrono::milliseconds>(destruction_end - destruction_start);

    std::cout << "[TEST] Server connection destroyed in " << destruction_duration.count() << "ms\n";

    std::atomic<bool> server_stopped {false};

    server->set_stopped_callback(
        [&]()
        {
            server_stopped = true;
            std::cout << "[SERVER] Stopped\n";
        });

    server->async_stop();

    // // Wait for server to stop
    // auto stop_timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    // while (!server_stopped && std::chrono::steady_clock::now() < stop_timeout)
    // {
    //     std::this_thread::sleep_for(std::chrono::milliseconds(10));
    // }
    // ASSERT_TRUE(server_stopped) << "Server did not stop within timeout";

    // std::cout << "[TEST] Test completed successfully\n";
}

TEST_F(TcpServerLifeCycleTest, ServerConnectionShutdownWhileActiveTest)
{
    std::cout << "[TEST] Starting ServerConnectionShutdownWhileActiveTest\n";

    // This test destroys the server connection while async operations are still active
    TcpServer<> server(*io_context, 0);

    std::mutex connection_mutex;
    std::condition_variable connection_cv;
    std::shared_ptr<TcpConnection<>> server_connection;
    bool connection_received = false;

    server.set_connection_callback(
        [&](std::shared_ptr<TcpConnection<>> conn)
        {
            std::cout << "[SERVER] New connection accepted\n";
            std::unique_lock<std::mutex> lock(connection_mutex);
            server_connection   = conn;
            connection_received = true;
            connection_cv.notify_one();
        });

    server.async_start();
    unsigned short port = server.port();
    std::cout << "[SERVER] Listening on port " << port << "\n";

    // Create client
    boost::asio::ip::tcp::socket client_socket(*io_context);
    std::atomic<bool> client_connected {false};

    client_socket.async_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port),
                                [&](const boost::system::error_code& error)
                                {
                                    if (!error)
                                    {
                                        client_connected = true;
                                        std::cout << "[CLIENT] Connected\n";
                                    }
                                });

    // Wait for connection
    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!client_connected && std::chrono::steady_clock::now() < timeout)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(client_connected);

    {
        std::unique_lock<std::mutex> lock(connection_mutex);
        bool received = connection_cv.wait_for(lock, std::chrono::seconds(2), [&] { return connection_received; });
        ASSERT_TRUE(received);
    }

    ASSERT_NE(server_connection, nullptr);

    // Start receiving (this creates an active async operation)
    server_connection->start_receiving();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Destroy the connection WHILE the async_read is still pending
    std::cout << "[TEST] Destroying server connection with active async operations...\n";
    auto destruction_start = std::chrono::steady_clock::now();

    server_connection.reset(); // Should properly cancel and wait for operations

    auto destruction_end      = std::chrono::steady_clock::now();
    auto destruction_duration = std::chrono::duration_cast<std::chrono::milliseconds>(destruction_end - destruction_start);

    std::cout << "[TEST] Server connection destroyed in " << destruction_duration.count() << "ms\n";

    // Destruction should complete within reasonable time (not hang)
    EXPECT_LT(destruction_duration.count(), 2000) << "Destruction took too long (possible deadlock)";

    // Cleanup
    client_socket.close();

    std::atomic<bool> server_stopped {false};

    server.set_stopped_callback([&]() { server_stopped = true; });

    timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!server_stopped && std::chrono::steady_clock::now() < timeout)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "[TEST] Test completed\n";
}

TEST_F(TcpServerLifeCycleTest, ServerConnectionShutdownWithSendingTest)
{
    std::cout << "[TEST] Starting ServerConnectionShutdownWithSendingTest\n";

    // This test destroys the connection while a send operation is pending
    TcpServer<> server(*io_context, 0);

    std::mutex connection_mutex;
    std::condition_variable connection_cv;
    std::shared_ptr<TcpConnection<>> server_connection;
    bool connection_received = false;

    server.set_connection_callback(
        [&](std::shared_ptr<TcpConnection<>> conn)
        {
            std::unique_lock<std::mutex> lock(connection_mutex);
            server_connection   = conn;
            connection_received = true;
            connection_cv.notify_one();
        });

    server.async_start();
    unsigned short port = server.port();

    // Create client
    boost::asio::ip::tcp::socket client_socket(*io_context);
    std::atomic<bool> client_connected {false};

    client_socket.async_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port),
                                [&](const boost::system::error_code& error)
                                {
                                    if (!error)
                                    {
                                        client_connected = true;
                                    }
                                });

    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!client_connected && std::chrono::steady_clock::now() < timeout)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(client_connected);

    {
        std::unique_lock<std::mutex> lock(connection_mutex);
        bool received = connection_cv.wait_for(lock, std::chrono::seconds(2), [&] { return connection_received; });
        ASSERT_TRUE(received);
    }

    ASSERT_NE(server_connection, nullptr);

    // Prepare data to send
    std::string large_message(10000, 'A'); // 10KB of data
    std::size_t writable_size = 0;
    std::uint8_t* write_head  = server_connection->send_buffer().get_write_head(writable_size);
    ASSERT_NE(write_head, nullptr);
    ASSERT_GE(writable_size, large_message.size());

    std::memcpy(write_head, large_message.data(), large_message.size());
    server_connection->send_buffer().commit(large_message.size());

    // Start sending
    server_connection->start_sending();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Destroy while sending
    std::cout << "[TEST] Destroying server connection with pending send...\n";
    auto destruction_start = std::chrono::steady_clock::now();

    server_connection.reset();

    auto destruction_end      = std::chrono::steady_clock::now();
    auto destruction_duration = std::chrono::duration_cast<std::chrono::milliseconds>(destruction_end - destruction_start);

    std::cout << "[TEST] Server connection destroyed in " << destruction_duration.count() << "ms\n";

    EXPECT_LT(destruction_duration.count(), 2000) << "Destruction took too long";

    // Cleanup
    client_socket.close();

    std::atomic<bool> server_stopped {false};
    server.set_stopped_callback([&]() { server_stopped = true; });

    timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!server_stopped && std::chrono::steady_clock::now() < timeout)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "[TEST] Test completed\n";
}
