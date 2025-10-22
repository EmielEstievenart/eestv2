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

class TcpConnectionLifeCycleTests : public ::testing::Test
{
protected:
    static constexpr std::chrono::milliseconds startup_delay {100};

    void SetUp() override
    {
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
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
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