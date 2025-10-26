#include "eestv/net/connection/tcp_client_connection.hpp"
#include "eestv/net/connection/tcp_server.hpp"
#include "eestv/logging/eestv_logging.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <vector>

void run_server_example()
{
    boost::asio::io_context io_context;

    // Create a TCP server on port 12345
    eestv::TcpServer<> server(io_context, 12345, std::chrono::seconds(5));

    EESTV_LOG_INFO("Server listening on port " << server.port());

    // Keep track of active connections
    std::vector<std::shared_ptr<eestv::TcpServerConnection<>>> connections;

    // Set callback for new connections
    server.set_connection_callback(
        [&connections](std::shared_ptr<eestv::TcpServerConnection<>> connection)
        {
            EESTV_LOG_INFO("Client connected!");

            // Set callback for when connection is lost
            connection->set_connection_lost_callback(
                [&connections, connection]()
                {
                    EESTV_LOG_INFO("Server: Client disconnected, cleaning up...");

                    // Remove from active connections list
                    auto it = std::find(connections.begin(), connections.end(), connection);
                    if (it != connections.end())
                    {
                        connections.erase(it);
                    }
                });

            // Set custom keep-alive callback (optional)
            // Returns: {should_send, data_to_send}
            connection->set_keep_alive_callback(
                []() -> std::pair<bool, std::vector<char>>
                {
                    // Example: send a custom protocol keep-alive message
                    std::string keep_alive_msg = "SERVER_KEEPALIVE\n";
                    std::vector<char> data(keep_alive_msg.begin(), keep_alive_msg.end());
                    return {true, data};
                });

            // Add to active connections
            connections.push_back(connection);

            EESTV_LOG_INFO("Active connections: " << connections.size());
        });

    // Start accepting connections
    server.start();

    io_context.run();
}

void run_client_example()
{
    boost::asio::io_context io_context;

    // Create endpoint to connect to
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), 12345);

    // Create a client connection (will automatically reconnect when lost)
    auto client_conn = std::make_shared<eestv::TcpClientConnection<>>(endpoint, io_context, std::chrono::seconds(5));

    // Configure reconnection behavior
    client_conn->set_auto_reconnect(true);
    client_conn->set_max_reconnect_attempts(-1); // Infinite retries

    // Set callback for when connection is lost
    client_conn->set_connection_lost_callback([]() { EESTV_LOG_INFO("Client: Connection lost, will attempt to reconnect..."); });

    // Set custom keep-alive callback (optional)
    // Returns: {should_send, data_to_send}
    client_conn->set_keep_alive_callback(
        []() -> std::pair<bool, std::vector<char>>
        {
            // Example: send a custom protocol keep-alive message
            std::string keep_alive_msg = "CLIENT_KEEPALIVE\n";
            std::vector<char> data(keep_alive_msg.begin(), keep_alive_msg.end());
            return {true, data};
        });

    // Initiate connection
    client_conn->connect();

    io_context.run();
}

int main(int argc, char* argv[])
{
    EESTV_SET_LOG_LEVEL(Debug);

    if (argc < 2)
    {
        std::cout << "Usage: " << argv[0] << " [server|client]\n";
        return 1;
    }

    std::string mode(argv[1]);

    try
    {
        if (mode == "server")
        {
            run_server_example();
        }
        else if (mode == "client")
        {
            run_client_example();
        }
        else
        {
            std::cout << "Invalid mode. Use 'server' or 'client'\n";
            return 1;
        }
    }
    catch (const std::exception& exception)
    {
        EESTV_LOG_ERROR("Exception: " << exception.what());
        return 1;
    }

    return 0;
}
