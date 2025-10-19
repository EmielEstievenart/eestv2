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

class TcpConnectionTest : public ::testing::Test
{
protected:
    static constexpr std::chrono::milliseconds startup_delay {100};

    void SetUp() override
    {
        io_context = std::make_unique<boost::asio::io_context>();
        io_thread  = std::thread(
            [this]()
            {
                io_context->run();
                std::cout << "Io context stopped \n";
            });
    }

    void TearDown() override
    {
        std::cout << "[TearDown] Starting cleanup..." << std::endl;

        if (io_thread.joinable())
        {
            io_thread.join();
        }
    }

    std::unique_ptr<boost::asio::io_context> io_context;
    std::thread io_thread;
};

// // Test: Basic TcpConnection construction and destruction
// TEST_F(TcpConnectionTest, BasicConstructionAndDestruction)
// {
//     std::cout << "[TEST] Starting BasicConstructionAndDestruction\n";

//     TcpServer<> server(*io_context, 0);
//     server.async_start();
//     std::this_thread::sleep_for(startup_delay);

//     unsigned short port = server.port();
//     std::cout << "[SERVER] Listening on port " << port << "\n";

//     std::mutex connection_mutex;
//     std::condition_variable connection_cv;
//     std::shared_ptr<TcpConnection<>> server_connection;
//     bool connection_received = false;

//     server.set_connection_callback(
//         [&](std::shared_ptr<TcpConnection<>> conn)
//         {
//             std::unique_lock<std::mutex> lock(connection_mutex);
//             server_connection   = conn;
//             connection_received = true;
//             connection_cv.notify_one();
//         });

//     // Create client
//     boost::asio::ip::tcp::socket client_socket(*io_context);
//     std::atomic<bool> client_connected {false};

//     client_socket.async_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port),
//                                 [&](const boost::system::error_code& error)
//                                 {
//                                     if (!error)
//                                     {
//                                         client_connected = true;
//                                     }
//                                 });

//     auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
//     while (!client_connected && std::chrono::steady_clock::now() < timeout)
//     {
//         std::this_thread::sleep_for(std::chrono::milliseconds(10));
//     }
//     ASSERT_TRUE(client_connected);

//     {
//         std::unique_lock<std::mutex> lock(connection_mutex);
//         bool received = connection_cv.wait_for(lock, std::chrono::seconds(2), [&] { return connection_received; });
//         ASSERT_TRUE(received);
//     }

//     ASSERT_NE(server_connection, nullptr);
//     std::cout << "[TEST] Connection established successfully\n";

//     // Test successful construction
//     EXPECT_NE(&server_connection->receive_buffer(), nullptr);
//     EXPECT_NE(&server_connection->send_buffer(), nullptr);

//     // Test destruction
//     server_connection.reset();
//     std::cout << "[TEST] Connection destroyed successfully\n";

//     client_socket.close();
//     SUCCEED();
// }

// // Test: Connection callbacks - data received and connection lost
// TEST_F(TcpConnectionTest, ConnectionCallbacks)
// {
//     std::cout << "[TEST] Starting ConnectionCallbacks\n";

//     TcpServer<> server(*io_context, 0);
//     server.async_start();
//     std::this_thread::sleep_for(startup_delay);

//     unsigned short port = server.port();

//     std::mutex connection_mutex;
//     std::condition_variable connection_cv;
//     std::shared_ptr<TcpConnection<>> server_connection;
//     bool connection_received = false;

//     server.set_connection_callback(
//         [&](std::shared_ptr<TcpConnection<>> conn)
//         {
//             std::unique_lock<std::mutex> lock(connection_mutex);
//             server_connection   = conn;
//             connection_received = true;
//             connection_cv.notify_one();
//         });

//     // Create client
//     boost::asio::ip::tcp::socket client_socket(*io_context);
//     std::atomic<bool> client_connected {false};

//     client_socket.async_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port),
//                                 [&](const boost::system::error_code& error)
//                                 {
//                                     if (!error)
//                                     {
//                                         client_connected = true;
//                                     }
//                                 });

//     auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
//     while (!client_connected && std::chrono::steady_clock::now() < timeout)
//     {
//         std::this_thread::sleep_for(std::chrono::milliseconds(10));
//     }
//     ASSERT_TRUE(client_connected);

//     {
//         std::unique_lock<std::mutex> lock(connection_mutex);
//         bool received = connection_cv.wait_for(lock, std::chrono::seconds(2), [&] { return connection_received; });
//         ASSERT_TRUE(received);
//     }

//     ASSERT_NE(server_connection, nullptr);

//     // Test data received callback
//     std::atomic<bool> data_received {false};
//     server_connection->set_data_received_callback([&]() { data_received = true; });

//     server_connection->start_receiving();
//     std::this_thread::sleep_for(std::chrono::milliseconds(50));

//     // Send data from client
//     std::string test_message = "Hello Server!";
//     boost::asio::write(client_socket, boost::asio::buffer(test_message));

//     timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
//     while (!data_received && std::chrono::steady_clock::now() < timeout)
//     {
//         std::this_thread::sleep_for(std::chrono::milliseconds(10));
//     }
//     EXPECT_TRUE(data_received) << "Data received callback not invoked";

//     // Test connection lost callback
//     std::atomic<bool> connection_lost {false};
//     server_connection->set_connection_lost_callback([&]() { connection_lost = true; });

//     // Close client socket to trigger connection lost
//     client_socket.close();

//     timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
//     while (!connection_lost && std::chrono::steady_clock::now() < timeout)
//     {
//         std::this_thread::sleep_for(std::chrono::milliseconds(10));
//     }
//     EXPECT_TRUE(connection_lost) << "Connection lost callback not invoked";

//     std::cout << "[TEST] Test completed\n";
// }

// // Test: Send and receive data through buffers
// TEST_F(TcpConnectionTest, SendAndReceiveData)
// {
//     std::cout << "[TEST] Starting SendAndReceiveData\n";

//     TcpServer<> server(*io_context, 0);
//     server.async_start();
//     std::this_thread::sleep_for(startup_delay);

//     unsigned short port = server.port();

//     std::mutex connection_mutex;
//     std::condition_variable connection_cv;
//     std::shared_ptr<TcpConnection<>> server_connection;
//     bool connection_received = false;

//     server.set_connection_callback(
//         [&](std::shared_ptr<TcpConnection<>> conn)
//         {
//             std::unique_lock<std::mutex> lock(connection_mutex);
//             server_connection   = conn;
//             connection_received = true;
//             connection_cv.notify_one();
//         });

//     // Create client
//     boost::asio::ip::tcp::socket client_socket(*io_context);
//     std::atomic<bool> client_connected {false};

//     client_socket.async_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port),
//                                 [&](const boost::system::error_code& error)
//                                 {
//                                     if (!error)
//                                     {
//                                         client_connected = true;
//                                     }
//                                 });

//     auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
//     while (!client_connected && std::chrono::steady_clock::now() < timeout)
//     {
//         std::this_thread::sleep_for(std::chrono::milliseconds(10));
//     }
//     ASSERT_TRUE(client_connected);

//     {
//         std::unique_lock<std::mutex> lock(connection_mutex);
//         bool received = connection_cv.wait_for(lock, std::chrono::seconds(2), [&] { return connection_received; });
//         ASSERT_TRUE(received);
//     }

//     ASSERT_NE(server_connection, nullptr);

//     // Test receiving data
//     std::atomic<bool> data_received {false};
//     server_connection->set_data_received_callback([&]() { data_received = true; });
//     server_connection->start_receiving();

//     std::string client_message = "Hello from client!";
//     boost::asio::write(client_socket, boost::asio::buffer(client_message));

//     timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
//     while (!data_received && std::chrono::steady_clock::now() < timeout)
//     {
//         std::this_thread::sleep_for(std::chrono::milliseconds(10));
//     }
//     ASSERT_TRUE(data_received);

//     // Verify received data
//     std::size_t readable_size     = 0;
//     const std::uint8_t* read_head = server_connection->receive_buffer().get_read_head(readable_size);
//     ASSERT_NE(read_head, nullptr);
//     ASSERT_GE(readable_size, client_message.size());

//     std::string received_message(reinterpret_cast<const char*>(read_head), client_message.size());
//     EXPECT_EQ(received_message, client_message);
//     server_connection->receive_buffer().consume(client_message.size());

//     // Test sending data
//     std::string server_message = "Hello from server!";
//     std::size_t writable_size  = 0;
//     std::uint8_t* write_head   = server_connection->send_buffer().get_write_head(writable_size);
//     ASSERT_NE(write_head, nullptr);
//     ASSERT_GE(writable_size, server_message.size());

//     std::memcpy(write_head, server_message.data(), server_message.size());
//     server_connection->send_buffer().commit(server_message.size());
//     server_connection->start_sending();

//     // Receive on client
//     std::vector<char> client_receive_buffer(server_message.size());
//     boost::system::error_code receive_error;
//     std::size_t bytes_received = boost::asio::read(client_socket, boost::asio::buffer(client_receive_buffer), receive_error);

//     ASSERT_FALSE(receive_error) << "Client receive error: " << receive_error.message();
//     ASSERT_EQ(bytes_received, server_message.size());

//     std::string client_received_message(client_receive_buffer.begin(), client_receive_buffer.end());
//     EXPECT_EQ(client_received_message, server_message);

//     client_socket.close();
//     std::cout << "[TEST] Test completed\n";
// }

// // Test: Graceful disconnect
// TEST_F(TcpConnectionTest, GracefulDisconnect)
// {
//     std::cout << "[TEST] Starting GracefulDisconnect\n";

//     TcpServer<> server(*io_context, 0);
//     server.async_start();
//     std::this_thread::sleep_for(startup_delay);

//     unsigned short port = server.port();

//     std::mutex connection_mutex;
//     std::condition_variable connection_cv;
//     std::shared_ptr<TcpConnection<>> server_connection;
//     bool connection_received = false;

//     server.set_connection_callback(
//         [&](std::shared_ptr<TcpConnection<>> conn)
//         {
//             std::unique_lock<std::mutex> lock(connection_mutex);
//             server_connection   = conn;
//             connection_received = true;
//             connection_cv.notify_one();
//         });

//     // Create client
//     boost::asio::ip::tcp::socket client_socket(*io_context);
//     std::atomic<bool> client_connected {false};

//     client_socket.async_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port),
//                                 [&](const boost::system::error_code& error)
//                                 {
//                                     if (!error)
//                                     {
//                                         client_connected = true;
//                                     }
//                                 });

//     auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
//     while (!client_connected && std::chrono::steady_clock::now() < timeout)
//     {
//         std::this_thread::sleep_for(std::chrono::milliseconds(10));
//     }
//     ASSERT_TRUE(client_connected);

//     {
//         std::unique_lock<std::mutex> lock(connection_mutex);
//         bool received = connection_cv.wait_for(lock, std::chrono::seconds(2), [&] { return connection_received; });
//         ASSERT_TRUE(received);
//     }

//     ASSERT_NE(server_connection, nullptr);

//     // Call async disconnect
//     server_connection->asycn_disconnect();
//     std::this_thread::sleep_for(std::chrono::milliseconds(100));

//     // Verify client detects disconnect
//     std::vector<char> buffer(10);
//     boost::system::error_code error;
//     std::size_t bytes_read = client_socket.read_some(boost::asio::buffer(buffer), error);

//     // Should get EOF or connection reset
//     EXPECT_TRUE(error == boost::asio::error::eof || error == boost::asio::error::connection_reset || bytes_read == 0);

//     client_socket.close();
//     std::cout << "[TEST] Test completed\n";
// }

// // Test: Multiple send operations
// TEST_F(TcpConnectionTest, MultipleSendOperations)
// {
//     std::cout << "[TEST] Starting MultipleSendOperations\n";

//     TcpServer<> server(*io_context, 0);
//     server.async_start();
//     std::this_thread::sleep_for(startup_delay);

//     unsigned short port = server.port();

//     std::mutex connection_mutex;
//     std::condition_variable connection_cv;
//     std::shared_ptr<TcpConnection<>> server_connection;
//     bool connection_received = false;

//     server.set_connection_callback(
//         [&](std::shared_ptr<TcpConnection<>> conn)
//         {
//             std::unique_lock<std::mutex> lock(connection_mutex);
//             server_connection   = conn;
//             connection_received = true;
//             connection_cv.notify_one();
//         });

//     // Create client
//     boost::asio::ip::tcp::socket client_socket(*io_context);
//     std::atomic<bool> client_connected {false};

//     client_socket.async_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port),
//                                 [&](const boost::system::error_code& error)
//                                 {
//                                     if (!error)
//                                     {
//                                         client_connected = true;
//                                     }
//                                 });

//     auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
//     while (!client_connected && std::chrono::steady_clock::now() < timeout)
//     {
//         std::this_thread::sleep_for(std::chrono::milliseconds(10));
//     }
//     ASSERT_TRUE(client_connected);

//     {
//         std::unique_lock<std::mutex> lock(connection_mutex);
//         bool received = connection_cv.wait_for(lock, std::chrono::seconds(2), [&] { return connection_received; });
//         ASSERT_TRUE(received);
//     }

//     ASSERT_NE(server_connection, nullptr);

//     // Send multiple messages
//     std::vector<std::string> messages = {"Message1", "Message2", "Message3"};
//     std::string combined_message;

//     for (const auto& msg : messages)
//     {
//         combined_message += msg;
//         std::size_t writable_size = 0;
//         std::uint8_t* write_head  = server_connection->send_buffer().get_write_head(writable_size);
//         ASSERT_NE(write_head, nullptr);
//         ASSERT_GE(writable_size, msg.size());

//         std::memcpy(write_head, msg.data(), msg.size());
//         server_connection->send_buffer().commit(msg.size());
//         server_connection->start_sending();
//     }

//     // Receive all data on client
//     std::vector<char> receive_buffer(combined_message.size());
//     boost::system::error_code error;
//     std::size_t bytes_received = boost::asio::read(client_socket, boost::asio::buffer(receive_buffer), error);

//     ASSERT_FALSE(error) << "Receive error: " << error.message();
//     ASSERT_EQ(bytes_received, combined_message.size());

//     std::string received_data(receive_buffer.begin(), receive_buffer.end());
//     EXPECT_EQ(received_data, combined_message);

//     client_socket.close();
//     std::cout << "[TEST] Test completed\n";
// }

// // Moved from TcpServerLifeCycleTest
// TEST_F(TcpConnectionTest, ConnectionShutdownWhileActiveTest)
// {
//     std::cout << "[TEST] Starting ConnectionShutdownWhileActiveTest\n";

//     // This test destroys the server connection while async operations are still active
//     TcpServer<> server(*io_context, 0);

//     std::mutex connection_mutex;
//     std::condition_variable connection_cv;
//     std::shared_ptr<TcpConnection<>> server_connection;
//     bool connection_received = false;

//     server.set_connection_callback(
//         [&](std::shared_ptr<TcpConnection<>> conn)
//         {
//             std::cout << "[SERVER] New connection accepted\n";
//             std::unique_lock<std::mutex> lock(connection_mutex);
//             server_connection   = conn;
//             connection_received = true;
//             connection_cv.notify_one();
//         });

//     server.async_start();
//     unsigned short port = server.port();
//     std::cout << "[SERVER] Listening on port " << port << "\n";

//     // Create client
//     boost::asio::ip::tcp::socket client_socket(*io_context);
//     std::atomic<bool> client_connected {false};

//     client_socket.async_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port),
//                                 [&](const boost::system::error_code& error)
//                                 {
//                                     if (!error)
//                                     {
//                                         client_connected = true;
//                                         std::cout << "[CLIENT] Connected\n";
//                                     }
//                                 });

//     // Wait for connection
//     auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
//     while (!client_connected && std::chrono::steady_clock::now() < timeout)
//     {
//         std::this_thread::sleep_for(std::chrono::milliseconds(10));
//     }
//     ASSERT_TRUE(client_connected);

//     {
//         std::unique_lock<std::mutex> lock(connection_mutex);
//         bool received = connection_cv.wait_for(lock, std::chrono::seconds(2), [&] { return connection_received; });
//         ASSERT_TRUE(received);
//     }

//     ASSERT_NE(server_connection, nullptr);

//     // Start receiving (this creates an active async operation)
//     server_connection->start_receiving();
//     std::this_thread::sleep_for(std::chrono::milliseconds(50));

//     // Destroy the connection WHILE the async_read is still pending
//     std::cout << "[TEST] Destroying server connection with active async operations...\n";
//     auto destruction_start = std::chrono::steady_clock::now();

//     server_connection.reset(); // Should properly cancel and wait for operations

//     auto destruction_end      = std::chrono::steady_clock::now();
//     auto destruction_duration = std::chrono::duration_cast<std::chrono::milliseconds>(destruction_end - destruction_start);

//     std::cout << "[TEST] Server connection destroyed in " << destruction_duration.count() << "ms\n";

//     // Destruction should complete within reasonable time (not hang)
//     EXPECT_LT(destruction_duration.count(), 2000) << "Destruction took too long (possible deadlock)";

//     // Cleanup
//     client_socket.close();

//     std::atomic<bool> server_stopped {false};

//     server.set_stopped_callback([&]() { server_stopped = true; });

//     timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
//     while (!server_stopped && std::chrono::steady_clock::now() < timeout)
//     {
//         std::this_thread::sleep_for(std::chrono::milliseconds(10));
//     }

//     std::cout << "[TEST] Test completed\n";
// }

// // Moved from TcpServerLifeCycleTest
// TEST_F(TcpConnectionTest, ConnectionShutdownWithSendingTest)
// {
//     std::cout << "[TEST] Starting ConnectionShutdownWithSendingTest\n";

//     // This test destroys the connection while a send operation is pending
//     TcpServer<> server(*io_context, 0);

//     std::mutex connection_mutex;
//     std::condition_variable connection_cv;
//     std::shared_ptr<TcpConnection<>> server_connection;
//     bool connection_received = false;

//     server.set_connection_callback(
//         [&](std::shared_ptr<TcpConnection<>> conn)
//         {
//             std::unique_lock<std::mutex> lock(connection_mutex);
//             server_connection   = conn;
//             connection_received = true;
//             connection_cv.notify_one();
//         });

//     server.async_start();
//     unsigned short port = server.port();

//     // Create client
//     boost::asio::ip::tcp::socket client_socket(*io_context);
//     std::atomic<bool> client_connected {false};

//     client_socket.async_connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port),
//                                 [&](const boost::system::error_code& error)
//                                 {
//                                     if (!error)
//                                     {
//                                         client_connected = true;
//                                     }
//                                 });

//     auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
//     while (!client_connected && std::chrono::steady_clock::now() < timeout)
//     {
//         std::this_thread::sleep_for(std::chrono::milliseconds(10));
//     }
//     ASSERT_TRUE(client_connected);

//     {
//         std::unique_lock<std::mutex> lock(connection_mutex);
//         bool received = connection_cv.wait_for(lock, std::chrono::seconds(2), [&] { return connection_received; });
//         ASSERT_TRUE(received);
//     }

//     ASSERT_NE(server_connection, nullptr);

//     // Prepare data to send (use a size that fits in default buffer)
//     std::string large_message(4000, 'A'); // 4KB of data (fits in default 4096 buffer)
//     std::size_t writable_size = 0;
//     std::uint8_t* write_head  = server_connection->send_buffer().get_write_head(writable_size);
//     ASSERT_NE(write_head, nullptr);
//     ASSERT_GE(writable_size, large_message.size());

//     std::memcpy(write_head, large_message.data(), large_message.size());
//     server_connection->send_buffer().commit(large_message.size());

//     // Start sending
//     server_connection->start_sending();
//     std::this_thread::sleep_for(std::chrono::milliseconds(50));

//     // Destroy while sending
//     std::cout << "[TEST] Destroying server connection with pending send...\n";
//     auto destruction_start = std::chrono::steady_clock::now();

//     server_connection.reset();

//     auto destruction_end      = std::chrono::steady_clock::now();
//     auto destruction_duration = std::chrono::duration_cast<std::chrono::milliseconds>(destruction_end - destruction_start);

//     std::cout << "[TEST] Server connection destroyed in " << destruction_duration.count() << "ms\n";

//     EXPECT_LT(destruction_duration.count(), 2000) << "Destruction took too long";

//     // Cleanup
//     client_socket.close();

//     std::atomic<bool> server_stopped {false};
//     server.set_stopped_callback([&]() { server_stopped = true; });

//     timeout = std::chrono::steady_clock::now() + std::chrono::seconds(2);
//     while (!server_stopped && std::chrono::steady_clock::now() < timeout)
//     {
//         std::this_thread::sleep_for(std::chrono::milliseconds(10));
//     }

//     std::cout << "[TEST] Test completed\n";
// }
