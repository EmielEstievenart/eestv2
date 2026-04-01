#include "eestv/serial/multiplexing_serializer.hpp"
#include "eestv/serial/demultiplexing_deserializer.hpp"
#include "eestv/data/linear_buffer.hpp"
#include <iostream>
#include <cstdint>
#include <variant>

// Example message types
struct SensorData
{
    std::uint32_t sensor_id;
    std::int32_t temperature; // Temperature in millidegrees Celsius (e.g., 23500 = 23.5°C)
    std::int32_t humidity;    // Humidity in per-mille (e.g., 652 = 65.2%)

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

struct StatusMessage
{
    bool is_active;
    std::uint8_t error_code;
    std::uint64_t timestamp;

    template <typename Archive>
    void serialize(Archive& ar)
    {
        ar & is_active & error_code & timestamp;
    }
};

int main()
{
    using namespace eestv;

    std::cout << "=== Serialization Multiplexer Example ===\n\n";

    // Create a linear buffer
    LinearBuffer buffer(1024);

    // Create multiplexer and demultiplexer with our message types
    MultiplexingSerializer<SensorData, CommandMessage, StatusMessage> mux(buffer);
    DemultiplexingDeserializer<SensorData, CommandMessage, StatusMessage> demux(buffer);

    // Serialize different message types
    std::cout << "Serializing messages...\n";

    SensorData sensor {42, 23500, 652};
    if (mux.serialize(sensor))
    {
        std::cout << "  ✓ SensorData serialized\n";
    }

    CommandMessage command {5, 100, 255};
    if (mux.serialize(command))
    {
        std::cout << "  ✓ CommandMessage serialized\n";
    }

    StatusMessage status {true, 0, 1234567890};
    if (mux.serialize(status))
    {
        std::cout << "  ✓ StatusMessage serialized\n";
    }

    SensorData sensor2 {99, 18300, 457};
    if (mux.serialize(sensor2))
    {
        std::cout << "  ✓ Second SensorData serialized\n";
    }

    // Deserialize messages
    std::cout << "\nDeserializing messages...\n";

    for (int i = 0; i < 4; ++i)
    {
        bool success = false;
        auto variant = demux.deserialize(success);

        if (!success)
        {
            std::cout << "  ✗ Deserialization failed\n";
            break;
        }

        // Use std::visit to handle the variant
        std::visit(
            [i](auto&& msg)
            {
                using T = std::decay_t<decltype(msg)>;
                if constexpr (std::is_same_v<T, SensorData>)
                {
                    std::cout << "  [" << i << "] SensorData: id=" << msg.sensor_id << ", temp=" << (msg.temperature / 1000.0)
                              << "°C, humidity=" << (msg.humidity / 10.0) << "%\n";
                }
                else if constexpr (std::is_same_v<T, CommandMessage>)
                {
                    std::cout << "  [" << i << "] CommandMessage: cmd=" << static_cast<int>(msg.command_type)
                              << ", target=" << msg.target_id << ", param=" << msg.parameter << "\n";
                }
                else if constexpr (std::is_same_v<T, StatusMessage>)
                {
                    std::cout << "  [" << i << "] StatusMessage: active=" << msg.is_active << ", error=" << static_cast<int>(msg.error_code)
                              << ", timestamp=" << msg.timestamp << "\n";
                }
            },
            variant);
    }

    std::cout << "\n=== Example Complete ===\n";

    return 0;
}
