#include "eestv/net/connection/tcp_connection.hpp"
#include "eestv/net/connection/tcp_server.hpp"
#include <boost/asio.hpp>
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

using namespace eestv;

class TcpConnectionLifeCycleTests : public ::testing::Test
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
                std::cout << "IO context stopped\n";
            });
    }

    void TearDown() override
    {
        std::cout << "[TearDown] Starting cleanup...\n";

        work_guard.reset();
        io_context->stop();
        if (io_thread.joinable())
        {
            io_thread.join();
        }

        std::cout << "[TearDown] Cleanup complete\n";
    }

    std::unique_ptr<boost::asio::io_context> io_context;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard;
    std::thread io_thread;
};

// Test: Basic TcpConnection construction and destruction
TEST_F(TcpConnectionLifeCycleTests, BasicConstructionAndDestruction)
{
    std::cout << "[TEST] Starting BasicConstructionAndDestruction\n";

    // Create server on IO context
    TcpServer<> server(*io_context, 0);
    server.async_start();
    std::this_thread::sleep_for(startup_delay);

    unsigned short port = server.port();
    std::cout << "[SERVER] Listening on port " << port << "\n";

    std::mutex connection_mutex;
    std::condition_variable connection_cv;
    std::unique_ptr<TcpConnection<>> connection_from_server;
    bool connection_received = false;

    server.set_connection_callback(
        [&](std::unique_ptr<TcpConnection<>> new_connection)
        {
            std::unique_lock<std::mutex> lock(connection_mutex);
            connection_from_server = std::move(new_connection);
            connection_received    = true;
            connection_cv.notify_one();
        });

    // Create client on IO context
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
        std::this_thread::sleep_for(short_delay);
    }
    ASSERT_TRUE(client_connected);

    {
        std::unique_lock<std::mutex> lock(connection_mutex);
        bool received = connection_cv.wait_for(lock, std::chrono::seconds(2), [&] { return connection_received; });
        ASSERT_TRUE(received);
    }

    ASSERT_NE(connection_from_server, nullptr);
    std::cout << "[TEST] Connection established successfully\n";

    SUCCEED();
}

// Test: Start receiving and immediately destruct
TEST_F(TcpConnectionLifeCycleTests, StartReceivingAndDestruct)
{
    std::cout << "[TEST] Starting StartReceivingAndDestruct\n";

    TcpServer<> server(*io_context, 0);
    server.async_start();
    std::this_thread::sleep_for(startup_delay);

    unsigned short port = server.port();

    std::mutex connection_mutex;
    std::condition_variable connection_cv;
    std::unique_ptr<TcpConnection<>> connection_from_server;
    bool connection_received = false;

    server.set_connection_callback(
        [&](std::unique_ptr<TcpConnection<>> new_connection)
        {
            std::unique_lock<std::mutex> lock(connection_mutex);
            connection_from_server = std::move(new_connection);
            connection_received    = true;
            connection_cv.notify_one();
        });

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
        std::this_thread::sleep_for(short_delay);
    }
    ASSERT_TRUE(client_connected);

    {
        std::unique_lock<std::mutex> lock(connection_mutex);
        bool received = connection_cv.wait_for(lock, std::chrono::seconds(2), [&] { return connection_received; });
        ASSERT_TRUE(received);
    }

    ASSERT_NE(connection_from_server, nullptr);

    // Start receiving and immediately destruct
    ASSERT_NO_THROW(connection_from_server->start_receiving());
    ASSERT_NO_THROW(connection_from_server.reset());

    std::cout << "[TEST] StartReceivingAndDestruct completed\n";
    SUCCEED();
}

// Test: Start sending and immediately destruct
TEST_F(TcpConnectionLifeCycleTests, StartSendingAndDestruct)
{
    TcpServer<> server(*io_context, 0);
    server.async_start();
    std::this_thread::sleep_for(startup_delay);

    unsigned short port = server.port();

    std::mutex connection_mutex;
    std::condition_variable connection_cv;
    std::unique_ptr<TcpConnection<>> connection_from_server;
    bool connection_received = false;

    server.set_connection_callback(
        [&](std::unique_ptr<TcpConnection<>> new_connection)
        {
            std::unique_lock<std::mutex> lock(connection_mutex);
            connection_from_server = std::move(new_connection);
            connection_received    = true;
            connection_cv.notify_one();
        });

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
        std::this_thread::sleep_for(short_delay);
    }
    ASSERT_TRUE(client_connected);

    {
        std::unique_lock<std::mutex> lock(connection_mutex);
        bool received = connection_cv.wait_for(lock, std::chrono::seconds(2), [&] { return connection_received; });
        ASSERT_TRUE(received);
    }

    ASSERT_NE(connection_from_server, nullptr);

    // Put some data in the send buffer and start sending
    const std::string test_data = "test_data";
    std::size_t writable_size   = 0;
    std::uint8_t* write_head    = connection_from_server->send_buffer().get_write_head(writable_size);
    ASSERT_NE(write_head, nullptr);
    ASSERT_GE(writable_size, test_data.size());
    std::memcpy(write_head, test_data.data(), test_data.size());
    connection_from_server->send_buffer().commit(test_data.size());

    ASSERT_NO_THROW(connection_from_server->start_sending());
    ASSERT_NO_THROW(connection_from_server.reset());

    SUCCEED();
}

// Test: Start both send and receive, then destruct
TEST_F(TcpConnectionLifeCycleTests, StartBothAndDestruct)
{
    std::cout << "[TEST] Starting StartBothAndDestruct\n";

    TcpServer<> server(*io_context, 0);
    server.async_start();
    std::this_thread::sleep_for(startup_delay);

    unsigned short port = server.port();

    std::mutex connection_mutex;
    std::condition_variable connection_cv;
    std::unique_ptr<TcpConnection<>> connection_from_server;
    bool connection_received = false;

    server.set_connection_callback(
        [&](std::unique_ptr<TcpConnection<>> new_connection)
        {
            std::unique_lock<std::mutex> lock(connection_mutex);
            connection_from_server = std::move(new_connection);
            connection_received    = true;
            connection_cv.notify_one();
        });

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
        std::this_thread::sleep_for(short_delay);
    }
    ASSERT_TRUE(client_connected);

    {
        std::unique_lock<std::mutex> lock(connection_mutex);
        bool received = connection_cv.wait_for(lock, std::chrono::seconds(2), [&] { return connection_received; });
        ASSERT_TRUE(received);
    }

    ASSERT_NE(connection_from_server, nullptr);

    // Put some data in the send buffer
    const std::string test_data = "test_data";
    std::size_t writable_size   = 0;
    std::uint8_t* write_head    = connection_from_server->send_buffer().get_write_head(writable_size);
    ASSERT_NE(write_head, nullptr);
    ASSERT_GE(writable_size, test_data.size());
    std::memcpy(write_head, test_data.data(), test_data.size());
    connection_from_server->send_buffer().commit(test_data.size());

    // Start both operations and immediately destruct
    ASSERT_NO_THROW(connection_from_server->start_receiving());
    ASSERT_NO_THROW(connection_from_server->start_sending());
    ASSERT_NO_THROW(connection_from_server.reset());

    std::cout << "[TEST] StartBothAndDestruct completed\n";
    SUCCEED();
}

// Test: Async stop with callback
TEST_F(TcpConnectionLifeCycleTests, AsyncStopWithCallback)
{
    std::cout << "[TEST] Starting AsyncStopWithCallback\n";

    TcpServer<> server(*io_context, 0);
    server.async_start();
    std::this_thread::sleep_for(startup_delay);

    unsigned short port = server.port();

    std::mutex connection_mutex;
    std::condition_variable connection_cv;
    std::unique_ptr<TcpConnection<>> connection_from_server;
    bool connection_received = false;

    server.set_connection_callback(
        [&](std::unique_ptr<TcpConnection<>> new_connection)
        {
            std::unique_lock<std::mutex> lock(connection_mutex);
            connection_from_server = std::move(new_connection);
            connection_received    = true;
            connection_cv.notify_one();
        });

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
        std::this_thread::sleep_for(short_delay);
    }
    ASSERT_TRUE(client_connected);

    {
        std::unique_lock<std::mutex> lock(connection_mutex);
        bool received = connection_cv.wait_for(lock, std::chrono::seconds(2), [&] { return connection_received; });
        ASSERT_TRUE(received);
    }

    ASSERT_NE(connection_from_server, nullptr);

    // Start operations
    connection_from_server->start_receiving();
    std::this_thread::sleep_for(medium_delay);

    std::atomic<bool> stopped {false};
    bool stop_initiated = connection_from_server->async_stop([&stopped]() { stopped = true; });

    EXPECT_TRUE(stop_initiated);

    auto start_time = std::chrono::steady_clock::now();
    while (!stopped && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2))
    {
        std::this_thread::sleep_for(short_delay);
    }

    EXPECT_TRUE(stopped);
    std::cout << "[TEST] AsyncStopWithCallback completed\n";
    SUCCEED();
}

// Test: Synchronous stop
TEST_F(TcpConnectionLifeCycleTests, SyncStop)
{
    std::cout << "[TEST] Starting SyncStop\n";

    TcpServer<> server(*io_context, 0);
    server.async_start();
    std::this_thread::sleep_for(startup_delay);

    unsigned short port = server.port();

    std::mutex connection_mutex;
    std::condition_variable connection_cv;
    std::unique_ptr<TcpConnection<>> connection_from_server;
    bool connection_received = false;

    server.set_connection_callback(
        [&](std::unique_ptr<TcpConnection<>> new_connection)
        {
            std::unique_lock<std::mutex> lock(connection_mutex);
            connection_from_server = std::move(new_connection);
            connection_received    = true;
            connection_cv.notify_one();
        });

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
        std::this_thread::sleep_for(short_delay);
    }
    ASSERT_TRUE(client_connected);

    {
        std::unique_lock<std::mutex> lock(connection_mutex);
        bool received = connection_cv.wait_for(lock, std::chrono::seconds(2), [&] { return connection_received; });
        ASSERT_TRUE(received);
    }

    ASSERT_NE(connection_from_server, nullptr);

    // Start operations
    connection_from_server->start_receiving();
    std::this_thread::sleep_for(medium_delay);

    // Synchronous stop should block until stopped
    ASSERT_NO_THROW(connection_from_server->stop());

    std::cout << "[TEST] SyncStop completed\n";
    SUCCEED();
}

// Test: Destruction without starting any operations
TEST_F(TcpConnectionLifeCycleTests, DestructionWithoutStart)
{
    std::cout << "[TEST] Starting DestructionWithoutStart\n";

    TcpServer<> server(*io_context, 0);
    server.async_start();
    std::this_thread::sleep_for(startup_delay);

    unsigned short port = server.port();

    std::mutex connection_mutex;
    std::condition_variable connection_cv;
    std::unique_ptr<TcpConnection<>> connection_from_server;
    bool connection_received = false;

    server.set_connection_callback(
        [&](std::unique_ptr<TcpConnection<>> new_connection)
        {
            std::unique_lock<std::mutex> lock(connection_mutex);
            connection_from_server = std::move(new_connection);
            connection_received    = true;
            connection_cv.notify_one();
        });

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
        std::this_thread::sleep_for(short_delay);
    }
    ASSERT_TRUE(client_connected);

    {
        std::unique_lock<std::mutex> lock(connection_mutex);
        bool received = connection_cv.wait_for(lock, std::chrono::seconds(2), [&] { return connection_received; });
        ASSERT_TRUE(received);
    }

    ASSERT_NE(connection_from_server, nullptr);

    // Destruct without starting any operations
    ASSERT_NO_THROW(connection_from_server.reset());

    std::cout << "[TEST] DestructionWithoutStart completed\n";
    SUCCEED();
}

// Test: Multiple start_receiving calls
TEST_F(TcpConnectionLifeCycleTests, MultipleStartReceivingCalls)
{
    std::cout << "[TEST] Starting MultipleStartReceivingCalls\n";

    TcpServer<> server(*io_context, 0);
    server.async_start();
    std::this_thread::sleep_for(startup_delay);

    unsigned short port = server.port();

    std::mutex connection_mutex;
    std::condition_variable connection_cv;
    std::unique_ptr<TcpConnection<>> connection_from_server;
    bool connection_received = false;

    server.set_connection_callback(
        [&](std::unique_ptr<TcpConnection<>> new_connection)
        {
            std::unique_lock<std::mutex> lock(connection_mutex);
            connection_from_server = std::move(new_connection);
            connection_received    = true;
            connection_cv.notify_one();
        });

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
        std::this_thread::sleep_for(short_delay);
    }
    ASSERT_TRUE(client_connected);

    {
        std::unique_lock<std::mutex> lock(connection_mutex);
        bool received = connection_cv.wait_for(lock, std::chrono::seconds(2), [&] { return connection_received; });
        ASSERT_TRUE(received);
    }

    ASSERT_NE(connection_from_server, nullptr);

    // Multiple start_receiving calls should be safe
    ASSERT_NO_THROW(connection_from_server->start_receiving());
    std::this_thread::sleep_for(medium_delay);
    ASSERT_NO_THROW(connection_from_server->start_receiving());
    ASSERT_NO_THROW(connection_from_server->start_receiving());

    connection_from_server->stop();

    std::cout << "[TEST] MultipleStartReceivingCalls completed\n";
    SUCCEED();
}

// Test: Multiple start_sending calls
TEST_F(TcpConnectionLifeCycleTests, MultipleStartSendingCalls)
{
    std::cout << "[TEST] Starting MultipleStartSendingCalls\n";

    TcpServer<> server(*io_context, 0);
    server.async_start();
    std::this_thread::sleep_for(startup_delay);

    unsigned short port = server.port();

    std::mutex connection_mutex;
    std::condition_variable connection_cv;
    std::unique_ptr<TcpConnection<>> connection_from_server;
    bool connection_received = false;

    server.set_connection_callback(
        [&](std::unique_ptr<TcpConnection<>> new_connection)
        {
            std::unique_lock<std::mutex> lock(connection_mutex);
            connection_from_server = std::move(new_connection);
            connection_received    = true;
            connection_cv.notify_one();
        });

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
        std::this_thread::sleep_for(short_delay);
    }
    ASSERT_TRUE(client_connected);

    {
        std::unique_lock<std::mutex> lock(connection_mutex);
        bool received = connection_cv.wait_for(lock, std::chrono::seconds(2), [&] { return connection_received; });
        ASSERT_TRUE(received);
    }

    ASSERT_NE(connection_from_server, nullptr);

    // Put some data in the send buffer
    const std::string test_data = "test_data";
    std::size_t writable_size   = 0;
    std::uint8_t* write_head    = connection_from_server->send_buffer().get_write_head(writable_size);
    ASSERT_NE(write_head, nullptr);
    ASSERT_GE(writable_size, test_data.size());
    std::memcpy(write_head, test_data.data(), test_data.size());
    connection_from_server->send_buffer().commit(test_data.size());

    // Multiple start_sending calls should be safe
    ASSERT_NO_THROW(connection_from_server->start_sending());
    std::this_thread::sleep_for(medium_delay);
    ASSERT_NO_THROW(connection_from_server->start_sending());
    ASSERT_NO_THROW(connection_from_server->start_sending());

    connection_from_server->stop();

    std::cout << "[TEST] MultipleStartSendingCalls completed\n";
    SUCCEED();
}

// Test: Start operations after stop
TEST_F(TcpConnectionLifeCycleTests, StartAfterStop)
{
    std::cout << "[TEST] Starting StartAfterStop\n";

    TcpServer<> server(*io_context, 0);
    server.async_start();
    std::this_thread::sleep_for(startup_delay);

    unsigned short port = server.port();

    std::mutex connection_mutex;
    std::condition_variable connection_cv;
    std::unique_ptr<TcpConnection<>> connection_from_server;
    bool connection_received = false;

    server.set_connection_callback(
        [&](std::unique_ptr<TcpConnection<>> new_connection)
        {
            std::unique_lock<std::mutex> lock(connection_mutex);
            connection_from_server = std::move(new_connection);
            connection_received    = true;
            connection_cv.notify_one();
        });

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
        std::this_thread::sleep_for(short_delay);
    }
    ASSERT_TRUE(client_connected);

    {
        std::unique_lock<std::mutex> lock(connection_mutex);
        bool received = connection_cv.wait_for(lock, std::chrono::seconds(2), [&] { return connection_received; });
        ASSERT_TRUE(received);
    }

    ASSERT_NE(connection_from_server, nullptr);

    // Start and stop operations
    connection_from_server->start_receiving();
    std::this_thread::sleep_for(medium_delay);

    std::atomic<bool> stopped {false};
    connection_from_server->async_stop([&stopped]() { stopped = true; });

    auto start_time = std::chrono::steady_clock::now();
    while (!stopped && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2))
    {
        std::this_thread::sleep_for(short_delay);
    }

    ASSERT_TRUE(stopped);

    // Try to start operations after stop - these should be ignored
    ASSERT_NO_THROW(connection_from_server->start_receiving());
    ASSERT_NO_THROW(connection_from_server->start_sending());

    std::this_thread::sleep_for(long_delay);

    std::cout << "[TEST] StartAfterStop completed\n";
    SUCCEED();
}