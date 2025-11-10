#include <gtest/gtest.h>
#include "eestv/net/network_utils.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>

using namespace eestv;

class NetworkUtilsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Setup code if needed
    }

    void TearDown() override
    {
        // Cleanup code if needed
    }

    boost::asio::io_context io_context;
};

// Test that we can determine local address for localhost
TEST_F(NetworkUtilsTest, GetLocalAddressForLocalhost)
{
    // Create a remote endpoint pointing to localhost
    boost::asio::ip::udp::endpoint remote_endpoint(boost::asio::ip::make_address("127.0.0.1"), 12345);

    // Get the local address that would be used to reach this endpoint
    auto local_address = get_local_address_for_remote(io_context, remote_endpoint);

    // Should succeed
    ASSERT_TRUE(local_address.has_value());

    // Should return localhost since we're connecting to localhost
    EXPECT_EQ(*local_address, "127.0.0.1");
}

// Test that we can determine local address for a remote IP
TEST_F(NetworkUtilsTest, GetLocalAddressForRemoteIP)
{
    // Create a remote endpoint pointing to a public IP (Google DNS)
    boost::asio::ip::udp::endpoint remote_endpoint(boost::asio::ip::make_address("8.8.8.8"), 53);

    // Get the local address that would be used to reach this endpoint
    auto local_address = get_local_address_for_remote(io_context, remote_endpoint);

    // Should succeed (assuming internet connectivity)
    ASSERT_TRUE(local_address.has_value());

    // Should return a valid IP address (not localhost)
    EXPECT_FALSE(local_address->empty());

    // Should not be the remote address
    EXPECT_NE(*local_address, "8.8.8.8");
}

// Test with IPv6 localhost
TEST_F(NetworkUtilsTest, GetLocalAddressForIPv6Localhost)
{
    // Create a remote endpoint pointing to IPv6 localhost
    boost::asio::ip::udp::endpoint remote_endpoint(boost::asio::ip::make_address("::1"), 12345);

    // Get the local address that would be used to reach this endpoint
    auto local_address = get_local_address_for_remote(io_context, remote_endpoint);

    // Should succeed
    ASSERT_TRUE(local_address.has_value());

    // Should return IPv6 localhost
    EXPECT_EQ(*local_address, "::1");
}

// Test that the function returns a valid local address for a LAN address
TEST_F(NetworkUtilsTest, GetLocalAddressForLANAddress)
{
    // Create a remote endpoint pointing to a typical LAN address
    boost::asio::ip::udp::endpoint remote_endpoint(boost::asio::ip::make_address("192.168.1.100"), 12345);

    // Get the local address that would be used to reach this endpoint
    auto local_address = get_local_address_for_remote(io_context, remote_endpoint);

    // Should succeed
    ASSERT_TRUE(local_address.has_value());

    // Should return a valid IP address
    EXPECT_FALSE(local_address->empty());

    // If we're on a typical LAN, the local address should be in the 192.168.x.x range
    // But this may vary, so we just check it's not empty and not the remote address
    EXPECT_NE(*local_address, "192.168.1.100");
}
