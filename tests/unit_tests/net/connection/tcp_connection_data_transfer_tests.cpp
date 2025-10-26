#include "eestv/data/linear_buffer.hpp"
#include "eestv/logging/eestv_logging.hpp"
#include "eestv/net/connection/tcp_client.hpp"
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
#include <vector>

using namespace eestv;

class TcpConnectionDataTransferTests : public ::testing::Test
{
protected:
    static constexpr std::chrono::milliseconds startup_delay {100};
    static constexpr std::chrono::milliseconds short_delay {10};
    static constexpr std::chrono::milliseconds medium_delay {50};
    static constexpr std::chrono::milliseconds long_delay {200};

    void SetUp() override
    {
        EESTV_SET_LOG_LEVEL(Trace);

        // Setup io_context and work guard
        io_context = std::make_unique<boost::asio::io_context>();
        work_guard = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(io_context->get_executor());
        io_thread  = std::thread(
            [this]()
            {
                io_context->run();
                EESTV_LOG_INFO("IO context stopped");
            });

        // Setup server
        server = std::make_unique<TcpServer<>>(*io_context, 0);
        server->async_start();
        std::this_thread::sleep_for(startup_delay);
        port = server->port();

        EESTV_LOG_INFO("[SETUP] Server listening on port " << port);

        // Setup server connection callback
        server->set_connection_callback(
            [this](std::unique_ptr<TcpConnection<>> new_connection)
            {
                std::unique_lock<std::mutex> lock(server_connection_mutex);
                server_connection          = std::move(new_connection);
                server_connection_received = true;
                server_connection_cv.notify_all();
                EESTV_LOG_INFO("[SERVER] Connection received");
            });

        // Setup and connect client
        client = std::make_unique<TcpClient>(*io_context);

        std::atomic<bool> client_connected {false};
        std::atomic<bool> client_error {false};

        bool connect_initiated = client->async_connect(
            "127.0.0.1", port,
            [this, &client_connected, &client_error](boost::asio::ip::tcp::socket&& socket, const boost::system::error_code& error)
            {
                if (error)
                {
                    EESTV_LOG_ERROR("[CLIENT] Connection error: " << error.message());
                    client_error = true;
                }
                else
                {
                    std::unique_lock<std::mutex> lock(client_connection_mutex);
                    client_connection = std::make_unique<TcpConnection<>>(std::move(socket), *io_context, 4096, 4096);
                    client_connected  = true;
                    EESTV_LOG_INFO("[CLIENT] Connection established");
                }
            });

        ASSERT_TRUE(connect_initiated);

        // Wait for connection to complete
        auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!client_connected && !client_error && std::chrono::steady_clock::now() < timeout)
        {
            std::this_thread::sleep_for(short_delay);
        }

        ASSERT_TRUE(client_connected);
        ASSERT_FALSE(client_error);

        // Wait for server to receive the connection
        {
            std::unique_lock<std::mutex> lock(server_connection_mutex);
            bool received = server_connection_cv.wait_for(lock, std::chrono::seconds(5), [this] { return server_connection_received; });
            ASSERT_TRUE(received);
        }

        ASSERT_NE(server_connection, nullptr);
        ASSERT_NE(client_connection, nullptr);

        EESTV_LOG_INFO("[SETUP] Complete - connections established");
    }

    void TearDown() override
    {
        EESTV_LOG_INFO("[TEARDOWN] Starting cleanup...");

        // Clean up connections
        if (client_connection)
        {
            client_connection->stop();
            client_connection.reset();
        }

        if (server_connection)
        {
            server_connection->stop();
            server_connection.reset();
        }

        // Clean up client and server
        client.reset();
        server.reset();

        // Clean up io_context
        work_guard.reset();
        io_context->stop();
        if (io_thread.joinable())
        {
            io_thread.join();
        }

        EESTV_LOG_INFO("[TEARDOWN] Cleanup complete");
    }

    // IO context management
    std::unique_ptr<boost::asio::io_context> io_context;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard;
    std::thread io_thread;

    // Server
    std::unique_ptr<TcpServer<>> server;
    unsigned short port;
    std::unique_ptr<TcpConnection<>> server_connection;
    std::mutex server_connection_mutex;
    std::condition_variable server_connection_cv;
    bool server_connection_received {false};

    // Client
    std::unique_ptr<TcpClient> client;
    std::unique_ptr<TcpConnection<>> client_connection;
    std::mutex client_connection_mutex;
};

// Test: Send data from client to server
TEST_F(TcpConnectionDataTransferTests, ClientToServerBasicSend)
{
    EESTV_LOG_INFO("[TEST] Starting ClientToServerBasicSend");

    const std::string test_message = "Hello from client!";

    // Setup server to receive data
    std::atomic<bool> data_received {false};
    std::atomic<bool> data_is_identical {false};

    server_connection->set_data_received_callback(
        [this, &data_received, test_message, &data_is_identical](LinearBuffer& receive_buffer)
        {
            std::size_t available_size    = 0;
            const std::uint8_t* read_head = receive_buffer.get_read_head(available_size);

            if (read_head && available_size > 0)
            {
                std::string received_data;
                received_data.assign(reinterpret_cast<const char*>(read_head), available_size);
                server_connection->receive_buffer().consume(available_size);
                data_received = true;
                EESTV_LOG_INFO("[SERVER] Received " << available_size << " bytes: " << received_data);
                if (received_data == test_message)
                {
                    data_is_identical = true;
                }
            }
        });

    server_connection->start_receiving();

    // Client sends data
    {
        std::unique_lock<std::mutex> lock(client_connection_mutex);

        client_connection->call_queue_send_function(
            [test_message](LinearBuffer& send_buffer)

            {
                std::size_t writable_size = 0;
                std::uint8_t* write_head  = send_buffer.get_write_head(writable_size);
                ASSERT_NE(write_head, nullptr);
                ASSERT_GE(writable_size, test_message.size());
                std::memcpy(write_head, test_message.data(), test_message.size());
                send_buffer.commit(test_message.size());
            });

        EESTV_LOG_INFO("[CLIENT] Queued " << test_message.size() << " bytes to send");
    }

    client_connection->start_sending();

    // Wait for data to be received
    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!data_received && std::chrono::steady_clock::now() < timeout)
    {
        std::this_thread::sleep_for(short_delay);
    }

    ASSERT_TRUE(data_received);
    ASSERT_TRUE(data_is_identical.load());

    EESTV_LOG_INFO("[TEST] ClientToServerBasicSend completed successfully");
    SUCCEED();
}
