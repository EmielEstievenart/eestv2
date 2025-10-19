#include "eestv/net/discovery/udp_discovery_client.hpp"
#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using eestv::UdpDiscoveryClient;

int main(int argc, char* argv[])
{
    try
    {
        // Parse command line arguments
        std::string service_identifier = "database"; // Default service
        int port                       = 12345;      // Default port
        int timeout_ms                 = 2000;       // Default timeout

        if (argc > 1)
        {
            service_identifier = argv[1];
        }
        if (argc > 2)
        {
            port = std::stoi(argv[2]);
        }
        if (argc > 3)
        {
            timeout_ms = std::stoi(argv[3]);
        }

        std::cout << "UDP Discovery Client" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "Searching for service: " << service_identifier << std::endl;
        std::cout << "Discovery port: " << port << std::endl;
        std::cout << "Timeout: " << timeout_ms << " ms" << std::endl;
        std::cout << "----------------------------------------" << std::endl;

        boost::asio::io_context io_context;

        std::atomic<bool> service_found {false};
        std::atomic<int> response_count {0};

        // Create the discovery client with a callback
        UdpDiscoveryClient client(
            io_context, service_identifier, std::chrono::milliseconds(timeout_ms), port,
            [&service_found, &response_count](const std::string& response, const boost::asio::ip::udp::endpoint& endpoint) -> bool
            {
                response_count++;
                std::cout << "\n✓ Service found!" << std::endl;
                std::cout << "  Response: " << response << std::endl;
                std::cout << "  From: " << endpoint.address().to_string() << ":" << endpoint.port() << std::endl;

                service_found = true;

                // Return false to stop searching after first response
                // Return true to continue listening for more responses
                return true;
            });

        // Start the client
        client.async_start();

        // Run the IO context in a separate thread
        std::thread io_thread(
            [&io_context]()
            {
                auto work_guard = boost::asio::make_work_guard(io_context);
                io_context.run();
            });

        std::cout << "\nSearching for service..." << std::endl;

        // Wait for discovery to complete or timeout
        auto start_time = std::chrono::steady_clock::now();
        auto max_wait   = std::chrono::milliseconds(timeout_ms + 1000); // Give extra time

        while (!service_found && std::chrono::steady_clock::now() - start_time < max_wait)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Stop the client
        client.stop();
        io_context.stop();

        if (io_thread.joinable())
        {
            io_thread.join();
        }

        std::cout << "\n----------------------------------------" << std::endl;
        if (service_found)
        {
            std::cout << "Discovery completed successfully!" << std::endl;
            std::cout << "Total responses received: " << response_count << std::endl;
        }
        else
        {
            std::cout << "Service '" << service_identifier << "' not found." << std::endl;
            std::cout << "Make sure the discovery server is running." << std::endl;
        }
        std::cout << "----------------------------------------" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
