#include "eestv/net/discovery/discoverable.hpp"
#include "eestv/net/discovery/udp_discovery_client.hpp"
#include "eestv/net/discovery/udp_discovery_server.hpp"
#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <vector>

namespace
{
const int test_port                    = 54322;
const std::string test_service1        = "database_service";
const std::string test_service2        = "api_service";
const std::string test_reply1          = "127.0.0.1:5432";
const std::string test_reply2          = "127.0.0.1:8080";
const std::string non_existent_service = "missing_service";
} // namespace

using eestv::Discoverable;
using eestv::UdpDiscoveryClient;
using eestv::UdpDiscoveryServer;

/**
 * Integration tests for UDP discovery system - tests realistic multi-component scenarios
 */
class DiscoveryIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        io_thread = std::thread(
            [this]()
            {
                auto work_guard = boost::asio::make_work_guard(io_context);
                io_context.run();
            });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    void TearDown() override
    {
        if (client)
        {
            client->stop();
            client.reset();
        }

        if (server)
        {
            server.reset();
        }

        io_context.stop();

        if (io_thread.joinable())
        {
            io_thread.join();
        }

        io_context.restart();
    }

    boost::asio::io_context io_context;
    std::unique_ptr<UdpDiscoveryServer> server;
    std::unique_ptr<UdpDiscoveryClient> client;
    std::thread io_thread;
};

TEST_F(DiscoveryIntegrationTest, SingleServiceDiscovery)
{
    Discoverable service(test_service1, []() { return test_reply1; });
    server = std::make_unique<UdpDiscoveryServer>(io_context, test_port);
    server->add_discoverable(service);
    server->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::atomic<bool> found {false};
    std::string received_reply;

    client =
        std::make_unique<UdpDiscoveryClient>(io_context, test_service1, std::chrono::milliseconds(500), test_port,
                                             [&found, &received_reply](const std::string& response, const boost::asio::ip::udp::endpoint&)
                                             {
                                                 received_reply = response;
                                                 found          = true;
                                                 return true;
                                             });

    client->async_start();

    auto start_time = std::chrono::steady_clock::now();
    while (!found && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ASSERT_TRUE(found) << "Service was not discovered within timeout";
    EXPECT_EQ(received_reply, test_reply1);
}

TEST_F(DiscoveryIntegrationTest, MultipleServicesDiscovery)
{
    Discoverable service1(test_service1, []() { return test_reply1; });
    Discoverable service2(test_service2, []() { return test_reply2; });

    server = std::make_unique<UdpDiscoveryServer>(io_context, test_port);
    server->add_discoverable(service1);
    server->add_discoverable(service2);
    server->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Discover first service
    {
        std::atomic<bool> found {false};
        std::string received_reply;

        client = std::make_unique<UdpDiscoveryClient>(
            io_context, test_service1, std::chrono::milliseconds(500), test_port,
            [&found, &received_reply](const std::string& response, const boost::asio::ip::udp::endpoint&)
            {
                received_reply = response;
                found          = true;
                return true;
            });

        client->async_start();

        auto start_time = std::chrono::steady_clock::now();
        while (!found && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        ASSERT_TRUE(found) << "First service was not discovered";
        EXPECT_EQ(received_reply, test_reply1);

        client->async_stop([]() { });
        client.reset();
    }

    // Discover second service
    {
        std::atomic<bool> found {false};
        std::string received_reply;

        client = std::make_unique<UdpDiscoveryClient>(
            io_context, test_service2, std::chrono::milliseconds(500), test_port,
            [&found, &received_reply](const std::string& response, const boost::asio::ip::udp::endpoint&)
            {
                received_reply = response;
                found          = true;
                return true;
            });

        client->async_start();

        auto start_time = std::chrono::steady_clock::now();
        while (!found && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        ASSERT_TRUE(found) << "Second service was not discovered";
        EXPECT_EQ(received_reply, test_reply2);
    }
}

TEST_F(DiscoveryIntegrationTest, NonexistentServiceNoResponse)
{
    Discoverable service(test_service1, []() { return test_reply1; });
    server = std::make_unique<UdpDiscoveryServer>(io_context, test_port);
    server->add_discoverable(service);
    server->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::atomic<bool> found {false};

    client = std::make_unique<UdpDiscoveryClient>(io_context, non_existent_service, std::chrono::milliseconds(300), test_port,
                                                  [&found](const std::string&, const boost::asio::ip::udp::endpoint&)
                                                  {
                                                      found = true;
                                                      return true;
                                                  });

    client->async_start();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    EXPECT_FALSE(found) << "Unexpectedly received response for nonexistent service";
}

TEST_F(DiscoveryIntegrationTest, DynamicCallbackReply)
{
    int call_count = 0;
    Discoverable service(test_service1, [&call_count]() { return "reply_" + std::to_string(++call_count); });

    server = std::make_unique<UdpDiscoveryServer>(io_context, test_port);
    server->add_discoverable(service);
    server->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // First discovery
    {
        std::atomic<bool> found {false};
        std::string received_reply;

        client = std::make_unique<UdpDiscoveryClient>(
            io_context, test_service1, std::chrono::milliseconds(500), test_port,
            [&found, &received_reply](const std::string& response, const boost::asio::ip::udp::endpoint&)
            {
                received_reply = response;
                found          = true;
                return true;
            });

        client->async_start();

        auto start_time = std::chrono::steady_clock::now();
        while (!found && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        ASSERT_TRUE(found);
        EXPECT_EQ(received_reply, "reply_1");

        client->async_stop([]() { });
        client.reset();
    }

    // Second discovery should get incremented reply
    {
        std::atomic<bool> found {false};
        std::string received_reply;

        client = std::make_unique<UdpDiscoveryClient>(
            io_context, test_service1, std::chrono::milliseconds(500), test_port,
            [&found, &received_reply](const std::string& response, const boost::asio::ip::udp::endpoint&)
            {
                received_reply = response;
                found          = true;
                return true;
            });

        client->async_start();

        auto start_time = std::chrono::steady_clock::now();
        while (!found && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        ASSERT_TRUE(found);
        EXPECT_EQ(received_reply, "reply_2");
    }
}

TEST_F(DiscoveryIntegrationTest, ClientRetryMechanism)
{
    std::thread delayed_server_thread(
        [this]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(800));

            Discoverable service(test_service1, []() { return test_reply1; });
            server = std::make_unique<UdpDiscoveryServer>(io_context, test_port);
            server->add_discoverable(service);
            server->start();
        });

    std::atomic<bool> found {false};
    std::string received_reply;

    client =
        std::make_unique<UdpDiscoveryClient>(io_context, test_service1, std::chrono::milliseconds(300), test_port,
                                             [&found, &received_reply](const std::string& response, const boost::asio::ip::udp::endpoint&)
                                             {
                                                 received_reply = response;
                                                 found          = true;
                                                 return true;
                                             });

    client->async_start();

    auto start_time = std::chrono::steady_clock::now();
    while (!found && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(3))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    delayed_server_thread.join();

    ASSERT_TRUE(found) << "Service was not discovered despite retries";
    EXPECT_EQ(received_reply, test_reply1);
}

TEST_F(DiscoveryIntegrationTest, ConcurrentClientRequests)
{
    Discoverable service(test_service1, []() { return test_reply1; });
    server = std::make_unique<UdpDiscoveryServer>(io_context, test_port);
    server->add_discoverable(service);
    server->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const int num_clients = 5;
    std::vector<std::atomic<bool>> results(num_clients);
    std::vector<std::string> replies(num_clients);

    std::vector<boost::asio::io_context> client_contexts(num_clients);
    std::vector<std::unique_ptr<UdpDiscoveryClient>> clients(num_clients);
    std::vector<std::thread> client_threads;

    for (int i = 0; i < num_clients; ++i)
    {
        results[i] = false;

        client_threads.emplace_back(
            [&, i]()
            {
                auto work_guard = boost::asio::make_work_guard(client_contexts[i]);

                clients[i] = std::make_unique<UdpDiscoveryClient>(
                    client_contexts[i], test_service1, std::chrono::milliseconds(500), test_port,
                    [&results, &replies, i](const std::string& response, const boost::asio::ip::udp::endpoint&)
                    {
                        replies[i] = response;
                        results[i] = true;
                        return true;
                    });

                clients[i]->async_start();
                client_contexts[i].run();
            });
    }

    auto start_time = std::chrono::steady_clock::now();
    bool all_found  = false;
    while (!all_found && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(3))
    {
        all_found = true;
        for (int i = 0; i < num_clients; ++i)
        {
            if (!results[i])
            {
                all_found = false;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    for (int i = 0; i < num_clients; ++i)
    {
        if (clients[i])
        {
            clients[i]->async_stop([]() { });
        }
        client_contexts[i].stop();
    }

    for (auto& thread : client_threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    for (int i = 0; i < num_clients; ++i)
    {
        EXPECT_TRUE(results[i]) << "Client " << i << " did not receive response";
        EXPECT_EQ(replies[i], test_reply1) << "Client " << i << " received incorrect reply";
    }
}
