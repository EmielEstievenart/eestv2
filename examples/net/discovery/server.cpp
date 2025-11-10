#include "eestv/net/discovery/discoverable.hpp"
#include "eestv/net/discovery/udp_discovery_server.hpp"
#include <boost/asio.hpp>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using eestv::Discoverable;
using eestv::UdpDiscoveryServer;

int main(int argc, char* argv[])
{
    try
    {
        // Parse command line arguments
        int port = 12345; // Default port
        if (argc > 1)
        {
            port = std::stoi(argv[1]);
        }

        std::cout << "Starting UDP Discovery Server on port " << port << std::endl;
        std::cout << "Press Ctrl+C to stop the server" << std::endl;
        std::cout << "----------------------------------------" << std::endl;

        boost::asio::io_context io_context;

        // Create the discovery server
        UdpDiscoveryServer server(io_context, port);

        // Add some example discoverable services
        Discoverable database_service("database",
                                      [](const auto& remote_endpoint) -> std::string
                                      {
                                          std::cout << "  -> Received request for 'database' service from "
                                                    << remote_endpoint.address().to_string() << ":" << remote_endpoint.port() << std::endl;
                                          return "127.0.0.1:5432";
                                      });

        Discoverable api_service("api",
                                 [](const auto& remote_endpoint) -> std::string
                                 {
                                     std::cout << "  -> Received request for 'api' service from "
                                               << remote_endpoint.address().to_string() << ":" << remote_endpoint.port() << std::endl;
                                     return "127.0.0.1:8080";
                                 });

        Discoverable web_service("web",
                                 [](const auto& remote_endpoint) -> std::string
                                 {
                                     std::cout << "  -> Received request for 'web' service from "
                                               << remote_endpoint.address().to_string() << ":" << remote_endpoint.port() << std::endl;
                                     return "127.0.0.1:3000";
                                 });

        // Add a dynamic service that returns the current timestamp
        Discoverable time_service("time",
                                  [](const auto& remote_endpoint) -> std::string
                                  {
                                      auto now       = std::chrono::system_clock::now();
                                      auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
                                      std::cout << "  -> Received request for 'time' service from "
                                                << remote_endpoint.address().to_string() << ":" << remote_endpoint.port() << std::endl;
                                      return "current_timestamp:" + std::to_string(timestamp);
                                  });

        server.add_discoverable(database_service);
        server.add_discoverable(api_service);
        server.add_discoverable(web_service);
        server.add_discoverable(time_service);

        std::cout << "Registered services:" << std::endl;
        std::cout << "  - database (returns: 127.0.0.1:5432)" << std::endl;
        std::cout << "  - api (returns: 127.0.0.1:8080)" << std::endl;
        std::cout << "  - web (returns: 127.0.0.1:3000)" << std::endl;
        std::cout << "  - time (returns: current timestamp)" << std::endl;
        std::cout << "----------------------------------------" << std::endl;

        // Start the server
        server.async_start();

        // Run the IO context in a separate thread
        std::thread io_thread(
            [&io_context]()
            {
                auto work_guard = boost::asio::make_work_guard(io_context);
                io_context.run();
            });

        std::cout << "Server is running and listening for discovery requests..." << std::endl;

        // Keep the main thread alive
        io_thread.join();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
