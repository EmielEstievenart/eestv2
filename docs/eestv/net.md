# Network Guide

This library provides asynchronous TCP connection setup, buffer-based TCP I/O, and UDP service discovery.

Use this API when:
- You want to accept incoming TCP connections with `TcpServer`.
- You want to initiate outgoing TCP connections with `TcpClient`.
- You want to send and receive raw bytes on an established socket with `TcpConnection`.
- You want to advertise or discover services over UDP with `UdpDiscoveryServer`, `UdpDiscoveryClient`, and `Discoverable`.

## Which API Should I Use?

| Need | Use |
| --- | --- |
| Accept incoming TCP connections | `TcpServer` |
| Connect to a remote TCP endpoint | `TcpClient` |
| Read and write data on an established TCP socket | `TcpConnection` |
| Advertise services over UDP | `UdpDiscoveryServer` + `Discoverable` |
| Search for services over UDP | `UdpDiscoveryClient` |
| Parse or construct a structured discovery reply string | `DiscoveryString` |

## Quick Start

### 1. Run a `boost::asio::io_context`

All network APIs here are asynchronous. They need a running `boost::asio::io_context`.

```cpp
#include <boost/asio.hpp>

boost::asio::io_context io_context;

io_context.run();
```

In a real application, you typically start your networking objects first and then run the `io_context`.

### 2. Accept incoming connections with `TcpServer`

`TcpServer` accepts sockets and gives you a `std::unique_ptr<TcpConnection<>>` for each new client.

```cpp
#include "eestv/net/connection/tcp_connection.hpp"
#include "eestv/net/connection/tcp_server.hpp"
#include <boost/asio.hpp>
#include <memory>
#include <vector>

boost::asio::io_context io_context;
eestv::TcpServer<> server(io_context, 12345);
std::vector<std::unique_ptr<eestv::TcpConnection<>>> connections;

server.set_connection_callback(
    [&connections](std::unique_ptr<eestv::TcpConnection<>> connection)
    {
        connection->set_connection_lost_callback(
            []()
            {
                // Handle disconnect
            });

        connection->start_receiving();
        connections.push_back(std::move(connection));
    });

bool started = server.async_start();
io_context.run();
```

### 3. Connect to a server with `TcpClient`

`TcpClient` resolves and connects. On success, its callback gives you a connected socket. You typically wrap that socket in `TcpConnection`.

```cpp
#include "eestv/net/connection/tcp_client.hpp"
#include "eestv/net/connection/tcp_connection.hpp"
#include <boost/asio.hpp>
#include <memory>

boost::asio::io_context io_context;
eestv::TcpClient client(io_context);
std::unique_ptr<eestv::TcpConnection<>> connection;

bool started = client.async_connect(
    "127.0.0.1", 12345,
    [&](boost::asio::ip::tcp::socket&& socket, const boost::system::error_code& error)
    {
        if (!error)
        {
            connection = std::make_unique<eestv::TcpConnection<>>(std::move(socket), io_context, 4096, 4096);
        }
    });

io_context.run();
```

### 4. Send and receive data with `TcpConnection`

`TcpConnection` works with raw bytes through its internal buffers.

```cpp
#include "eestv/net/connection/tcp_connection.hpp"
#include <cstdint>
#include <cstring>
#include <string>

void configure_connection(eestv::TcpConnection<>& connection)
{
    connection.set_data_received_callback(
        [](eestv::LinearBuffer& receive_buffer)
        {
            std::size_t available = 0;
            const std::uint8_t* read_head = receive_buffer.get_read_head(available);

            if (read_head != nullptr && available > 0)
            {
                std::string payload(reinterpret_cast<const char*>(read_head), available);
                receive_buffer.consume(available);
            }
        });

    connection.start_receiving();

    const std::string message = "hello";

    connection.call_queue_send_function(
        [&message](eestv::LinearBuffer& send_buffer)
        {
            std::size_t writable = 0;
            std::uint8_t* write_head = send_buffer.get_write_head(writable);

            if (write_head != nullptr && writable >= message.size())
            {
                std::memcpy(write_head, message.data(), message.size());
                send_buffer.commit(message.size());
            }
        });

    connection.start_sending();
}
```

### 5. Advertise a service with `UdpDiscoveryServer`

`Discoverable` maps a short service identifier to a reply string. `UdpDiscoveryServer` listens for those identifiers over UDP broadcast and replies with the discoverable's callback result.

```cpp
#include "eestv/net/discovery/discoverable.hpp"
#include "eestv/net/discovery/discovery_string.hpp"
#include "eestv/net/discovery/udp_discovery_server.hpp"
#include <boost/asio.hpp>

boost::asio::io_context io_context;
eestv::UdpDiscoveryServer server(io_context, 54321);

eestv::Discoverable api_service(
    "api",
    [](const boost::asio::ip::udp::endpoint& /* remote_endpoint */)
    {
        return eestv::DiscoveryString::construct("api", "192.168.1.100", 8080);
    });

server.add_discoverable(api_service);

bool started = server.async_start();
io_context.run();
```

### 6. Discover a service with `UdpDiscoveryClient`

`UdpDiscoveryClient` repeatedly broadcasts the service name until you stop it. The response callback receives the raw reply string and the responding endpoint.

```cpp
#include "eestv/net/discovery/discovery_string.hpp"
#include "eestv/net/discovery/udp_discovery_client.hpp"
#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <optional>
#include <thread>

boost::asio::io_context io_context;
auto work_guard = boost::asio::make_work_guard(io_context); // executor_work_guard<boost::asio::io_context::executor_type>
std::thread io_thread([&io_context]() { io_context.run(); });

std::atomic<bool> found {false};
std::optional<eestv::DiscoveryInfo> discovered;

eestv::UdpDiscoveryClient client(
    io_context, "api", std::chrono::milliseconds(500), 54321,
    [&found, &discovered](const std::string& response, const boost::asio::ip::udp::endpoint&)
    {
        discovered = eestv::DiscoveryString::parse(response); // std::optional<eestv::DiscoveryInfo>
        found = discovered.has_value();
        return discovered.has_value();
    });

bool started = client.async_start();

auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3); // std::chrono::time_point<std::chrono::steady_clock>
while (!found && std::chrono::steady_clock::now() < deadline)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

client.stop();
work_guard.reset();
io_context.stop();
io_thread.join();
```

## Important Constraints

### TCP Roles Are Split Across Three Types

- `TcpServer` accepts inbound connections.
- `TcpClient` creates outbound connections.
- `TcpConnection` is the object you use after a socket is connected.

`TcpClient` does not expose send/receive buffer operations directly. It only gives you a connected socket, which you then wrap in `TcpConnection`.

### `TcpConnection` Sends Raw Bytes

`TcpConnection` is a transport layer, not a message protocol layer.

That means:
- It sends whatever bytes are currently queued in the send buffer.
- It receives arbitrary byte chunks into the receive buffer.
- It does not add framing, typing, serialization, or message boundaries.

If you need typed messages, pair it with the serialization layer described in [serial.md](/c:/Users/emiel/Documents/Development/eestv/docs/eestv/serial.md).

### Data-Received Callback Restrictions

`TcpConnection::set_data_received_callback()` is called while the connection holds an internal lock.

Important consequences:
- Do not call methods on the same `TcpConnection` from inside that callback.
- Read or consume bytes from the provided `ReceiveBuffer`.
- If you need to trigger more work, hand the data off and react outside the callback.

### Send Workflow

To send bytes through `TcpConnection`:
- Queue bytes into the send buffer with `call_queue_send_function(...)`.
- Commit the queued bytes inside that callback.
- Call `start_sending()` to begin or continue asynchronous sending.

Writing directly to the send buffer without `call_queue_send_function(...)` is not the intended public workflow.

### Discovery Replies Are Application-Defined Strings

`Discoverable` returns an arbitrary reply string.

That means:
- The discovery server does not enforce a schema.
- You can return `"127.0.0.1:8080"`, JSON, or any other string format.
- `DiscoveryString` is an optional helper for the built-in `"SERVICE_NAME:IP:PORT:TIMESTAMP"` format.

### Discovery Client Retry Behavior

`UdpDiscoveryClient` rebroadcasts the service name every `retry_timeout` until you stop it.

Important detail:
- The callback return value is not currently a stop/continue control.
- The current implementation keeps listening and retrying until `stop()` or `async_stop(...)` is called.

### Stop and Reset Semantics

- `stop()` is synchronous and should not be called from the `io_context` thread.
- `async_stop(...)` is the asynchronous alternative when you are already inside asynchronous flow.
- `TcpClient` provides `reset()` if you want to reuse the same client object after a successful stop.
- `UdpDiscoveryClient` and `UdpDiscoveryServer` also provide `reset()` for reuse after stop.
- `TcpServer` does not provide `reset()`; create a new server object if you need a fresh server instance.

## Practical Notes

- All examples here use the default `LinearBuffer`-based `TcpConnection<>`.
- `TcpServer` hands off connections as `std::unique_ptr<TcpConnection<>>`, so you must take ownership in the connection callback.
- `TcpConnection::receive_buffer()` and `send_buffer()` expose the underlying buffers if you need direct access, but the normal send path is `call_queue_send_function(...)`.
- `TcpConnection::remote_endpoint()` gives you the connected peer endpoint after construction.
- For UDP discovery, keep callbacks lightweight. If you need to reconfigure or stop other networking objects in response, signal that work externally rather than doing deep control flow directly inside the callback.
