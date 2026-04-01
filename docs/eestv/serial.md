# Serialization Guide

This library serializes data directly into and out of `eestv::LinearBuffer`.

Use this API when:
- You want to write a known type with `Serializer` and read it back with `Deserializer`.
- You want to send multiple message types through one buffer with `MultiplexingSerializer` and `DemultiplexingDeserializer`.

## Which API Should I Use?

| Need | Use |
| --- | --- |
| Write one known type into a buffer | `Serializer` |
| Read one known type from a buffer | `Deserializer` |
| Write one of several registered message types into one buffer | `MultiplexingSerializer<Types...>` |
| Read one of several registered message types from one buffer | `DemultiplexingDeserializer<Types...>` |

## Quick Start

### 1. Make your type serializable

You can make a type serializable in either of these ways:
- Add a member `serialize` function.
- Add a free `serialize` function in the same namespace as the type.

The simplest approach is usually the member `serialize` function.

```cpp
#include <cstdint>

struct Point
{
    std::int32_t x;
    std::int32_t y;

    template <typename Archive>
    void serialize(Archive& ar)
    {
        ar & x & y;
    }
};
```

Free-function alternative:

```cpp
#include <cstdint>

namespace app
{

struct Point
{
    std::int32_t x;
    std::int32_t y;
};

template <typename Archive>
void serialize(Point& value, Archive& ar)
{
    ar & value.x & value.y;
}

} // namespace app
```

### 2. Serialize a value with `Serializer`

```cpp
#include "eestv/data/linear_buffer.hpp"
#include "eestv/serial/serializer.hpp"

eestv::LinearBuffer buffer(256);
eestv::Serializer serializer(buffer);

Point out {10, 20};
serializer & out;

std::size_t written = serializer.bytes_written();
```

`Serializer` writes at the current write head of the buffer.

### 3. Deserialize a value with `Deserializer`

```cpp
#include "eestv/data/linear_buffer.hpp"
#include "eestv/serial/serializer.hpp"
#include "eestv/serial/deserializer.hpp"

eestv::LinearBuffer buffer(256);

Point original {10, 20};
eestv::Serializer serializer(buffer);
serializer & original;

Point restored {};
eestv::Deserializer deserializer(buffer);
deserializer & restored;

std::size_t read = deserializer.bytes_read();
```

`Deserializer` reads from the current read head of the buffer.

### 4. Serialize multiple message types with `MultiplexingSerializer`

Use the multiplexing API when one buffer may contain different message types.

```cpp
#include <cstdint>

#include "eestv/data/linear_buffer.hpp"
#include "eestv/serial/serialization_mux.hpp"

struct Ping
{
    std::uint32_t id;

    template <typename Archive>
    void serialize(Archive& ar)
    {
        ar & id;
    }
};

struct Status
{
    bool ok;
    std::uint16_t code;

    template <typename Archive>
    void serialize(Archive& ar)
    {
        ar & ok & code;
    }
};

eestv::LinearBuffer buffer(1024);
eestv::MultiplexingSerializer<Ping, Status> mux(buffer);

bool ping_written = mux.serialize(Ping {42});
bool status_written = mux.serialize(Status {true, 7});
```

Each serialized message is written as:

```text
[1 byte type_index] [2 bytes payload_size] [N bytes serialized_payload]
```

### 5. Read multiple message types with `DemultiplexingDeserializer`

```cpp
#include "eestv/data/linear_buffer.hpp"
#include "eestv/serial/serialization_mux.hpp"
#include <type_traits>
#include <variant>

eestv::LinearBuffer buffer(1024);
eestv::MultiplexingSerializer<Ping, Status> mux(buffer);
eestv::DemultiplexingDeserializer<Ping, Status> demux(buffer);

mux.serialize(Ping {42});
mux.serialize(Status {true, 7});

bool success = false;
auto message = demux.deserialize(success); // std::variant<Ping, Status>

if (success)
{
    std::visit(
        [](const auto& value) // const Ping& or const Status&
        {
            using T = std::decay_t<decltype(value)>; // Ping or Status

            if constexpr (std::is_same_v<T, Ping>)
            {
                // Handle Ping
            }
            else if constexpr (std::is_same_v<T, Status>)
            {
                // Handle Status
            }
        },
        message);
}
```

`DemultiplexingDeserializer<Types...>` returns `std::variant<Types...>`. It sets `success` to `false` when the header is incomplete, the type index is invalid, or the payload does not match the expected size.

## Important Constraints

### Buffer Type

All four APIs currently operate on `eestv::LinearBuffer`.

```cpp
eestv::LinearBuffer buffer(1024);
```

### Primitive Encoding

Primitive arithmetic types are serialized by copying their in-memory bytes.

That means:
- The format is native-endian.
- The format depends on the platform representation of the serialized types.
- There is no built-in schema evolution or versioning layer.

This is appropriate when both sides agree on the exact layout. It is not a portable cross-platform interchange format by itself.

### Unsupported Primitive Types

`float`, `double`, and `long double` are explicitly rejected.

If you need to represent fractional values, use an integer encoding such as fixed-point or scaled integers.

### Failure Behavior of `Serializer` and `Deserializer`

`Serializer` and `Deserializer` do not return a success flag from `operator&`.

Important consequences:
- If the buffer does not have enough writable space, `Serializer` stops advancing.
- If the buffer does not have enough readable data, `Deserializer` stops advancing.
- You should size the buffer appropriately and inspect `bytes_written()` or `bytes_read()` when that matters.

### Rules for `MultiplexingSerializer` and `DemultiplexingDeserializer`

- Both sides must use the exact same type list in the exact same order.
- `MultiplexingSerializer<Types...>::serialize(value)` only accepts types that are in `Types...`.
- Provision enough buffer space for the full message before calling `serialize()`.
- `DemultiplexingDeserializer<Types...>` returns a `std::variant<Types...>`.
- `DemultiplexingDeserializer` default-constructs the selected type internally, so each registered type must be default-constructible.
- The type index is one byte, so at most 255 types can be registered.
- The payload size field is `uint16_t`, so one message payload can be at most 65535 bytes.

## Practical Notes

- Use plain `Serializer` and `Deserializer` when the sender and receiver already know the exact type at that point in the protocol.
- Use the multiplexing pair when one stream carries multiple message kinds and you need the payload to identify its own type.
- Reuse the same `serialize` implementation for both directions; the archive type decides whether bytes are written or read.
- If you only need the multiplexing pair, include `eestv/serial/serialization_mux.hpp`. It includes both `multiplexing_serializer.hpp` and `demultiplexing_deserializer.hpp`.
