# Serialization Multiplexer

A type-safe serialization system that enables multiplexing multiple message types over a single communication channel with automatic type identification and deserialization.

## Files

- **`serialization_mux_common.hpp`** - Shared constants and utilities
- **`serialization_multiplexer.hpp`** - The multiplexer for serialization
- **`serialization_demultiplexer.hpp`** - The demultiplexer for deserialization
- **`serialization_mux.hpp`** - Convenience header that includes both multiplexer and demultiplexer

## Overview

The serialization multiplexer allows you to:
- Serialize objects of different types into the same buffer
- Automatically add type metadata (type index + payload size)
- Deserialize messages and automatically reconstruct the correct type
- Use compile-time type safety with `std::variant`

## Message Format

Each serialized message has the following format:

```
[1 byte: type_index] [2 bytes: payload_size] [N bytes: serialized_payload]
```

- **type_index**: 0-based index of the type in the template parameter pack (max 255 types)
- **payload_size**: Little-endian uint16_t, maximum 65535 bytes
- **serialized_payload**: The actual serialized data

## Usage

### 1. Define Your Message Types

Each message type must implement a `serialize` method that works with both `Serializer` and `Deserializer`:

```cpp
struct SensorData
{
    std::uint32_t sensor_id;
    std::int32_t temperature;
    std::int32_t humidity;

    template <typename Archive>
    void serialize(Archive& ar)
    {
        ar & sensor_id & temperature & humidity;
    }
};

struct CommandMessage
{
    std::uint8_t command_type;
    std::uint32_t target_id;
    std::uint16_t parameter;

    template <typename Archive>
    void serialize(Archive& ar)
    {
        ar & command_type & target_id & parameter;
    }
};
```

### 2. Create a Storage Adapter

You need a storage adapter that implements `write()` and `read()` methods. For `LinearBuffer`:

```cpp
class LinearBufferAdapter
{
public:
    explicit LinearBufferAdapter(LinearBuffer& buffer) : _buffer(buffer) { }

    bool write(const void* data, std::size_t size)
    {
        std::size_t writable;
        std::uint8_t* write_head = _buffer.get_write_head(writable);
        if (write_head == nullptr || writable < size || data == nullptr || size == 0)
        {
            return false;
        }
        std::memcpy(write_head, data, size);
        return _buffer.commit(size);
    }

    bool read(void* data, std::size_t size)
    {
        std::size_t available;
        const std::uint8_t* ptr = _buffer.get_read_head(available);
        if (available < size)
        {
            return false;
        }
        std::memcpy(data, ptr, size);
        return _buffer.consume(size);
    }

private:
    LinearBuffer& _buffer;
};
```

### 3. Serialize Messages

```cpp
using namespace eestv;

LinearBuffer buffer(1024);
LinearBufferAdapter adapter(buffer);

// Create multiplexer with your message types
SerializationMultiplexer<LinearBufferAdapter, SensorData, CommandMessage> mux(adapter);

// Serialize different message types
SensorData sensor{42, 23500, 652};
if (mux.serialize(sensor))
{
    std::cout << "SensorData serialized\n";
}

CommandMessage command{5, 100, 255};
if (mux.serialize(command))
{
    std::cout << "CommandMessage serialized\n";
}
```

### 4. Deserialize Messages

```cpp
// Create demultiplexer with the SAME type list
SerializationDemultiplexer<LinearBufferAdapter, SensorData, CommandMessage> demux(adapter);

bool success = false;
auto variant = demux.deserialize(success);

if (success)
{
    // Use std::visit to handle the variant
    std::visit(
        [](auto&& msg)
        {
            using T = std::decay_t<decltype(msg)>;
            if constexpr (std::is_same_v<T, SensorData>)
            {
                std::cout << "Received SensorData: id=" << msg.sensor_id << "\n";
            }
            else if constexpr (std::is_same_v<T, CommandMessage>)
            {
                std::cout << "Received CommandMessage: cmd=" 
                          << static_cast<int>(msg.command_type) << "\n";
            }
        },
        variant);
}
```

## Important Constraints

1. **Type Order Matters**: The multiplexer and demultiplexer must have the **exact same type list in the same order**.

   ```cpp
   // Correct - same order on both sides
   SerializationMultiplexer<Storage, TypeA, TypeB, TypeC> mux(...);
   SerializationDemultiplexer<Storage, TypeA, TypeB, TypeC> demux(...);
   
   // WRONG - different order will deserialize to wrong types!
   SerializationMultiplexer<Storage, TypeA, TypeB, TypeC> mux(...);
   SerializationDemultiplexer<Storage, TypeB, TypeA, TypeC> demux(...);
   ```

2. **Maximum 255 Types**: The type index is a single byte, limiting you to 255 different message types.

3. **Maximum Payload Size**: 65535 bytes per message (uint16_t size field).

4. **Type Requirements**: Each message type must:
   - Be default constructible
   - Have a `serialize()` method that works with both serialization and deserialization
   - Only use allowed primitive types (no `float`, see `allowed_types.hpp`)

## How It Works

### Serialization Process

1. **Type Index Lookup**: At compile-time, the multiplexer determines which index in the type list corresponds to the serialized type.

2. **Two-Pass Serialization**:
   - First pass: Serialize to a temporary buffer to determine payload size
   - Second pass: Write header (type index + size) followed by payload to actual storage

3. **Header Writing**: Write 3-byte header with type index and little-endian size.

### Deserialization Process

1. **Read Header**: Read 3 bytes to get type index and payload size.

2. **Type Validation**: Verify type index is within bounds.

3. **Runtime Dispatch**: Use fold expressions to map the runtime type index to the correct compile-time type.

4. **Deserialize**: Create an instance of the correct type and deserialize into it.

5. **Return Variant**: Return a `std::variant<Types...>` containing the deserialized object.

## Example Output

```
=== Serialization Multiplexer Example ===

Serializing messages...
  ✓ SensorData serialized
  ✓ CommandMessage serialized
  ✓ StatusMessage serialized
  ✓ Second SensorData serialized

Deserializing messages...
  [0] SensorData: id=42, temp=23.5°C, humidity=65.2%
  [1] CommandMessage: cmd=5, target=100, param=255
  [2] StatusMessage: active=1, error=0, timestamp=1234567890
  [3] SensorData: id=99, temp=18.3°C, humidity=45.7%

=== Example Complete ===
```

## Advanced Usage

### Streaming Over Network

The serialization multiplexer is designed to work with streaming protocols:

```cpp
// Sender side
LinearBuffer send_buffer(4096);
LinearBufferAdapter send_adapter(send_buffer);
SerializationMultiplexer<...> mux(send_adapter);

while (true)
{
    // Serialize messages as they come
    mux.serialize(get_next_message());
    
    // Send buffer contents over network
    std::size_t available;
    const std::uint8_t* data = send_buffer.get_read_head(available);
    if (available > 0)
    {
        socket.send(data, available);
        send_buffer.consume(available);
    }
}

// Receiver side
LinearBuffer recv_buffer(4096);
LinearBufferAdapter recv_adapter(recv_buffer);
SerializationDemultiplexer<...> demux(recv_adapter);

while (true)
{
    // Receive data from network
    std::size_t writable;
    std::uint8_t* write_head = recv_buffer.get_write_head(writable);
    std::size_t received = socket.receive(write_head, writable);
    recv_buffer.commit(received);
    
    // Try to deserialize messages
    bool success = false;
    auto msg = demux.deserialize(success);
    if (success)
    {
        process_message(msg);
    }
}
```

## See Also

- `code/eestv/serial/serialization_multiplexer.hpp` - Multiplexer implementation
- `code/eestv/serial/serialization_demultiplexer.hpp` - Demultiplexer implementation
- `code/eestv/serial/serialization_mux_common.hpp` - Shared utilities
- `code/eestv/serial/serialization_mux.hpp` - Convenience header (includes both)
- `code/eestv/serial/serializer.hpp` - Base serialization system
- `code/eestv/serial/deserializer.hpp` - Base deserialization system
- `code/eestv/data/linear_buffer.hpp` - Linear buffer implementation
- `examples/multi_serializer/serialization_example.cpp` - Complete working example
