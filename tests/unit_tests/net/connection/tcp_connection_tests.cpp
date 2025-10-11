#include "eestv/net/connection/tcp_connection.hpp"
#include "eestv/net/connection/tcp_server.hpp"
#include "io_context_debugger.hpp"
#include <boost/asio.hpp>
#include <gtest/gtest.h>
#include <chrono>
#include <cstring>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>

using namespace eestv;

class TcpConnectionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        io_context = std::make_unique<boost::asio::io_context>();
        work_guard = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(io_context->get_executor());

        // Start io_context in background thread
        io_thread = std::thread([this]() { io_context->run(); });
    }

    void TearDown() override
    {
        std::cout << "[TearDown] Starting cleanup..." << std::endl;

        // Stop io_context and wait for thread to finish
        test::IoContextDebugger::print_state(*io_context, "Before work_guard reset");

        work_guard.reset();

        test::IoContextDebugger::print_state(*io_context, "After work_guard reset");

        // Wait for io_context to become idle (with timeout)
        std::cout << "[TearDown] Waiting for io_context to become idle..." << std::endl;
        bool idle = test::IoContextDebugger::wait_for_idle(*io_context, std::chrono::seconds(2));

        if (!idle)
        {
            std::cout << "[TearDown] WARNING: io_context did not become idle within timeout!" << std::endl;
            test::IoContextDebugger::force_stop_with_diagnostics(*io_context);
        }
        else
        {
            std::cout << "[TearDown] io_context became idle naturally" << std::endl;
        }

        io_context->stop();

        std::cout << "[TearDown] Joining io_thread..." << std::endl;
        if (io_thread.joinable())
        {
            io_thread.join();
        }
        std::cout << "[TearDown] Cleanup complete" << std::endl;
    }

    // Helper to write data to a buffer
    bool write_to_buffer(LinearBuffer& buffer, const std::string& data)
    {
        std::size_t writable;
        std::uint8_t* write_head = buffer.get_write_head(writable);
        if (write_head == nullptr || writable < data.size())
        {
            return false;
        }
        std::memcpy(write_head, data.c_str(), data.size());
        return buffer.commit(data.size());
    }

    // Helper to read data from a buffer
    std::string read_from_buffer(LinearBuffer& buffer)
    {
        std::size_t readable;
        const std::uint8_t* read_head = buffer.get_read_head(readable);
        if (read_head == nullptr || readable == 0)
        {
            return "";
        }
        std::string result(reinterpret_cast<const char*>(read_head), readable);
        buffer.consume(readable);
        return result;
    }

    static bool stop_server(TcpServer<>* server)
    {
        std::atomic<bool> stopped {false};
        server->async_stop(
            [&stopped]()
            {
                std::cout << "Server has stopped callback invoked\n";
                stopped = true;
            });

        for (int i = 0; i < 10 && !stopped; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        return stopped;
    }

    std::unique_ptr<boost::asio::io_context> io_context;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard;
    std::thread io_thread;
};

// Test basic server-client connection
TEST_F(TcpConnectionTest, BasicServerClientConnection)
{
    std::atomic<bool> server_connected {false};
    std::atomic<bool> client_connected {false};

    std::shared_ptr<TcpServer<>> server;
    std::shared_ptr<TcpClientConnection<>> client;
    std::shared_ptr<TcpServerConnection<>> server_connection;

    // Create server
    server = std::make_shared<TcpServer<>>(*io_context, 0); // Port 0 = any available port

    server->set_connection_callback(
        [&](std::shared_ptr<TcpServerConnection<>> connection)
        {
            server_connection = connection;
            connection->start_alive_monitoring();
            server_connected = true;
        });

    server->async_start();

    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Create client and connect
    unsigned short server_port = server->port();
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), server_port);

    client = std::make_shared<TcpClientConnection<>>(endpoint, *io_context);
    client->connect();

    // Wait for connection
    auto start = std::chrono::steady_clock::now();
    while (!server_connected && !client_connected && std::chrono::steady_clock::now() - start < std::chrono::seconds(5))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        client_connected = client->is_connected();
    }

    EXPECT_TRUE(server_connected);
    EXPECT_TRUE(client_connected);
    EXPECT_TRUE(client->is_connected());
    ASSERT_NE(server_connection, nullptr);
    EXPECT_TRUE(server_connection->is_connected());

    ASSERT_TRUE(stop_server(server.get()));
}

// Test sending data from client to server
TEST_F(TcpConnectionTest, ClientToServerDataTransfer)
{
    std::string received_data;
    std::mutex data_mutex;
    std::condition_variable data_cv;
    std::atomic<bool> data_received {false};

    std::shared_ptr<TcpServer<>> server;
    std::shared_ptr<TcpClientConnection<>> client;
    std::shared_ptr<TcpServerConnection<>> server_connection;

    // Create server
    server = std::make_shared<TcpServer<>>(*io_context, 0);

    server->set_connection_callback(
        [&](std::shared_ptr<TcpServerConnection<>> connection)
        {
            server_connection = connection;
            connection->start_alive_monitoring();
        });

    server->async_start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Create and connect client
    unsigned short server_port = server->port();
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), server_port);

    client = std::make_shared<TcpClientConnection<>>(endpoint, *io_context);
    client->connect();

    // Wait for connection
    auto start = std::chrono::steady_clock::now();
    while ((!server_connection || !client->is_connected()) && std::chrono::steady_clock::now() - start < std::chrono::seconds(5))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_NE(server_connection, nullptr);
    ASSERT_TRUE(client->is_connected());

    // Send data from client to server
    const std::string test_message = "Hello from client!";
    ASSERT_TRUE(write_to_buffer(client->send_buffer(), test_message));
    client->start_send();

    // Wait and read data on server side
    start = std::chrono::steady_clock::now();
    while (!data_received && std::chrono::steady_clock::now() - start < std::chrono::seconds(5))
    {
        std::string data = read_from_buffer(server_connection->receive_buffer());
        if (!data.empty())
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            received_data = data;
            data_received = true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(data_received);
    EXPECT_EQ(received_data, test_message);

    EXPECT_TRUE(server_connection->is_connected());

    server_connection->disconnect();
    client->disconnect();
}

// Test sending data from server to client
TEST_F(TcpConnectionTest, ServerToClientDataTransfer)
{
    std::string received_data;
    std::atomic<bool> data_received {false};

    std::shared_ptr<TcpServer<>> server;
    std::shared_ptr<TcpClientConnection<>> client;
    std::shared_ptr<TcpServerConnection<>> server_connection;

    // Create server
    server = std::make_shared<TcpServer<>>(*io_context, 0);

    server->set_connection_callback(
        [&](std::shared_ptr<TcpServerConnection<>> connection)
        {
            server_connection = connection;
            connection->start_alive_monitoring();
        });

    server->async_start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Create and connect client
    unsigned short server_port = server->port();
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), server_port);

    client = std::make_shared<TcpClientConnection<>>(endpoint, *io_context);
    client->connect();

    // Wait for connection
    auto start = std::chrono::steady_clock::now();
    while ((!server_connection || !client->is_connected()) && std::chrono::steady_clock::now() - start < std::chrono::seconds(5))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_NE(server_connection, nullptr);
    ASSERT_TRUE(client->is_connected());

    // Send data from server to client
    const std::string test_message = "Hello from server!";
    ASSERT_TRUE(write_to_buffer(server_connection->send_buffer(), test_message));
    server_connection->start_send();

    // Wait and read data on client side
    start = std::chrono::steady_clock::now();
    while (!data_received && std::chrono::steady_clock::now() - start < std::chrono::seconds(5))
    {
        std::string data = read_from_buffer(client->receive_buffer());
        if (!data.empty())
        {
            received_data = data;
            data_received = true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(data_received);
    EXPECT_EQ(received_data, test_message);
}

// Test bidirectional data transfer
TEST_F(TcpConnectionTest, BidirectionalDataTransfer)
{
    std::string client_received;
    std::string server_received;
    std::atomic<bool> client_data_received {false};
    std::atomic<bool> server_data_received {false};

    std::shared_ptr<TcpServer<>> server;
    std::shared_ptr<TcpClientConnection<>> client;
    std::shared_ptr<TcpServerConnection<>> server_connection;

    // Create server
    server = std::make_shared<TcpServer<>>(*io_context, 0);

    server->set_connection_callback(
        [&](std::shared_ptr<TcpServerConnection<>> connection)
        {
            server_connection = connection;
            connection->start_alive_monitoring();
        });

    server->async_start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Create and connect client
    unsigned short server_port = server->port();
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), server_port);

    client = std::make_shared<TcpClientConnection<>>(endpoint, *io_context);
    client->connect();

    // Wait for connection
    auto start = std::chrono::steady_clock::now();
    while ((!server_connection || !client->is_connected()) && std::chrono::steady_clock::now() - start < std::chrono::seconds(5))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_NE(server_connection, nullptr);
    ASSERT_TRUE(client->is_connected());

    // Send messages in both directions
    const std::string client_message = "Client says hello!";
    const std::string server_message = "Server says hi!";

    ASSERT_TRUE(write_to_buffer(client->send_buffer(), client_message));
    client->start_send();

    ASSERT_TRUE(write_to_buffer(server_connection->send_buffer(), server_message));
    server_connection->start_send();

    // Wait and read data on both sides
    start = std::chrono::steady_clock::now();
    while ((!client_data_received || !server_data_received) && std::chrono::steady_clock::now() - start < std::chrono::seconds(5))
    {
        if (!client_data_received)
        {
            std::string data = read_from_buffer(client->receive_buffer());
            if (!data.empty())
            {
                client_received      = data;
                client_data_received = true;
            }
        }

        if (!server_data_received)
        {
            std::string data = read_from_buffer(server_connection->receive_buffer());
            if (!data.empty())
            {
                server_received      = data;
                server_data_received = true;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(client_data_received);
    EXPECT_TRUE(server_data_received);
    EXPECT_EQ(client_received, server_message);
    EXPECT_EQ(server_received, client_message);
}

// Test keep-alive functionality with callback
TEST_F(TcpConnectionTest, KeepAliveWithCallback)
{
    std::atomic<int> client_keepalive_count {0};
    std::atomic<int> server_keepalive_count {0};
    std::atomic<int> server_received_keepalives {0};
    std::atomic<int> client_received_keepalives {0};

    std::shared_ptr<TcpServer<>> server;
    std::shared_ptr<TcpClientConnection<>> client;
    std::shared_ptr<TcpServerConnection<>> server_connection;

    // Create server with short keep-alive interval
    const std::chrono::seconds keepalive_interval {1};
    server = std::make_shared<TcpServer<>>(*io_context, 0, 4096, 4096, keepalive_interval);

    server->set_connection_callback(
        [&](std::shared_ptr<TcpServerConnection<>> connection)
        {
            server_connection = connection;

            // Set up keep-alive callback for server
            connection->set_keep_alive_callback(
                [&]() -> std::pair<bool, std::vector<char>>
                {
                    server_keepalive_count++;
                    std::string keepalive_msg = "SERVER_KEEPALIVE";
                    return {true, std::vector<char>(keepalive_msg.begin(), keepalive_msg.end())};
                });

            connection->start_alive_monitoring();
        });

    server->async_start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Create and connect client
    unsigned short server_port = server->port();
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), server_port);

    client = std::make_shared<TcpClientConnection<>>(endpoint, *io_context, 4096, 4096, keepalive_interval);

    // Set up keep-alive callback for client
    client->set_keep_alive_callback(
        [&]() -> std::pair<bool, std::vector<char>>
        {
            client_keepalive_count++;
            std::string keepalive_msg = "CLIENT_KEEPALIVE";
            return {true, std::vector<char>(keepalive_msg.begin(), keepalive_msg.end())};
        });

    client->connect();

    // Wait for connection
    auto start = std::chrono::steady_clock::now();
    while ((!server_connection || !client->is_connected()) && std::chrono::steady_clock::now() - start < std::chrono::seconds(5))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_NE(server_connection, nullptr);
    ASSERT_TRUE(client->is_connected());

    // Wait for keep-alive messages to be exchanged (at least 2 seconds to allow multiple keep-alives)
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    // Check for received keep-alive messages
    start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(2))
    {
        std::string client_data = read_from_buffer(client->receive_buffer());
        if (client_data.find("SERVER_KEEPALIVE") != std::string::npos)
        {
            client_received_keepalives++;
        }

        std::string server_data = read_from_buffer(server_connection->receive_buffer());
        if (server_data.find("CLIENT_KEEPALIVE") != std::string::npos)
        {
            server_received_keepalives++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Verify that keep-alive callbacks were called
    EXPECT_GT(client_keepalive_count.load(), 0) << "Client keep-alive callback should have been called";
    EXPECT_GT(server_keepalive_count.load(), 0) << "Server keep-alive callback should have been called";

    // Verify that keep-alive messages were received
    EXPECT_GT(client_received_keepalives.load(), 0) << "Client should have received keep-alive messages";
    EXPECT_GT(server_received_keepalives.load(), 0) << "Server should have received keep-alive messages";
}

// Test disabling keep-alive by returning false from callback
TEST_F(TcpConnectionTest, DisableKeepAliveViaCallback)
{
    std::atomic<int> keepalive_callback_count {0};
    std::atomic<int> received_keepalives {0};

    std::shared_ptr<TcpServer<>> server;
    std::shared_ptr<TcpClientConnection<>> client;
    std::shared_ptr<TcpServerConnection<>> server_connection;

    // Create server with short keep-alive interval
    const std::chrono::seconds keepalive_interval {1};
    server = std::make_shared<TcpServer<>>(*io_context, 0, 4096, 4096, keepalive_interval);

    server->set_connection_callback(
        [&](std::shared_ptr<TcpServerConnection<>> connection)
        {
            server_connection = connection;
            connection->start_alive_monitoring();
        });

    server->async_start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Create and connect client
    unsigned short server_port = server->port();
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), server_port);

    client = std::make_shared<TcpClientConnection<>>(endpoint, *io_context, 4096, 4096, keepalive_interval);

    // Set up keep-alive callback that disables keep-alive by returning false
    client->set_keep_alive_callback(
        [&]() -> std::pair<bool, std::vector<char>>
        {
            keepalive_callback_count++;
            return {false, std::vector<char>()}; // Disable keep-alive
        });

    client->connect();

    // Wait for connection
    auto start = std::chrono::steady_clock::now();
    while ((!server_connection || !client->is_connected()) && std::chrono::steady_clock::now() - start < std::chrono::seconds(5))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_NE(server_connection, nullptr);
    ASSERT_TRUE(client->is_connected());

    // Wait for potential keep-alive messages
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    // Check that no keep-alive messages were received on server
    start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(1))
    {
        std::string server_data = read_from_buffer(server_connection->receive_buffer());
        if (!server_data.empty())
        {
            received_keepalives++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Verify that callback was called (timer fired) but no keep-alive was sent
    EXPECT_GT(keepalive_callback_count.load(), 0) << "Keep-alive callback should have been called";
    EXPECT_EQ(received_keepalives.load(), 0) << "No keep-alive messages should have been received when disabled";
}

// Test connection lost detection
TEST_F(TcpConnectionTest, ConnectionLostDetection)
{
    std::atomic<bool> client_lost {false};
    std::atomic<bool> server_lost {false};

    std::shared_ptr<TcpServer<>> server;
    std::shared_ptr<TcpClientConnection<>> client;
    std::shared_ptr<TcpServerConnection<>> server_connection;

    // Create server
    server = std::make_shared<TcpServer<>>(*io_context, 0);

    server->set_connection_callback(
        [&](std::shared_ptr<TcpServerConnection<>> connection)
        {
            server_connection = connection;

            connection->set_connection_lost_callback([&]() { server_lost = true; });

            connection->start_alive_monitoring();
        });

    server->async_start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Create and connect client
    unsigned short server_port = server->port();
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), server_port);

    client = std::make_shared<TcpClientConnection<>>(endpoint, *io_context);

    client->set_connection_lost_callback([&]() { client_lost = true; });

    client->connect();

    // Wait for connection
    auto start = std::chrono::steady_clock::now();
    while ((!server_connection || !client->is_connected()) && std::chrono::steady_clock::now() - start < std::chrono::seconds(5))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_NE(server_connection, nullptr);
    ASSERT_TRUE(client->is_connected());

    // Disconnect client
    client->disconnect();

    // Wait for server to detect disconnection
    start = std::chrono::steady_clock::now();
    while (!server_lost && std::chrono::steady_clock::now() - start < std::chrono::seconds(5))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    EXPECT_TRUE(server_lost) << "Server should detect connection lost";
    EXPECT_FALSE(client->is_connected());
}

// Test multiple sequential messages
TEST_F(TcpConnectionTest, MultipleSequentialMessages)
{
    std::vector<std::string> received_messages;
    std::mutex messages_mutex;

    std::shared_ptr<TcpServer<>> server;
    std::shared_ptr<TcpClientConnection<>> client;
    std::shared_ptr<TcpServerConnection<>> server_connection;

    // Create server
    server = std::make_shared<TcpServer<>>(*io_context, 0);

    server->set_connection_callback(
        [&](std::shared_ptr<TcpServerConnection<>> connection)
        {
            server_connection = connection;
            connection->start_alive_monitoring();
        });

    server->async_start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Create and connect client
    unsigned short server_port = server->port();
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), server_port);

    client = std::make_shared<TcpClientConnection<>>(endpoint, *io_context);
    client->connect();

    // Wait for connection
    auto start = std::chrono::steady_clock::now();
    while ((!server_connection || !client->is_connected()) && std::chrono::steady_clock::now() - start < std::chrono::seconds(5))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_NE(server_connection, nullptr);
    ASSERT_TRUE(client->is_connected());

    // Send multiple messages
    const std::vector<std::string> test_messages = {"Message 1", "Message 2", "Message 3", "Message 4", "Message 5"};

    for (const auto& msg : test_messages)
    {
        ASSERT_TRUE(write_to_buffer(client->send_buffer(), msg));
        client->start_send();
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Small delay between messages
    }

    // Collect received messages
    start = std::chrono::steady_clock::now();
    while (received_messages.size() < test_messages.size() && std::chrono::steady_clock::now() - start < std::chrono::seconds(5))
    {
        std::string data = read_from_buffer(server_connection->receive_buffer());
        if (!data.empty())
        {
            std::lock_guard<std::mutex> lock(messages_mutex);
            received_messages.push_back(data);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_EQ(received_messages.size(), test_messages.size());
    for (size_t i = 0; i < test_messages.size() && i < received_messages.size(); ++i)
    {
        EXPECT_EQ(received_messages[i], test_messages[i]) << "Message " << i << " mismatch";
    }
}
