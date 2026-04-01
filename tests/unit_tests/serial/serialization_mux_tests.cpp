#include <gtest/gtest.h>
#include "eestv/serial/multiplexing_serializer.hpp"
#include "eestv/serial/demultiplexing_deserializer.hpp"
#include "eestv/data/linear_buffer.hpp"
#include <cstdint>
#include <variant>
#include <array>

namespace eestv
{

// Test message types
struct MessageA
{
    std::uint32_t id;
    std::uint16_t value;

    template <typename Archive>
    void serialize(Archive& ar)
    {
        ar & id & value;
    }

    bool operator==(const MessageA& other) const { return id == other.id && value == other.value; }
};

struct MessageB
{
    std::uint8_t command;
    std::int32_t data;
    bool flag;

    template <typename Archive>
    void serialize(Archive& ar)
    {
        ar & command & data & flag;
    }

    bool operator==(const MessageB& other) const { return command == other.command && data == other.data && flag == other.flag; }
};

struct MessageC
{
    std::uint64_t timestamp;

    template <typename Archive>
    void serialize(Archive& ar)
    {
        ar & timestamp;
    }

    bool operator==(const MessageC& other) const { return timestamp == other.timestamp; }
};

struct EmptyMessage
{
    template <typename Archive>
    void serialize(Archive& /* ar */)
    {
        // Empty message - no fields to serialize
    }

    bool operator==(const EmptyMessage& /* other */) const { return true; }
};

// Large message type for testing near size limits
struct LargeMessage
{
    std::array<std::uint64_t, 100> data; // 800 bytes

    template <typename Archive>
    void serialize(Archive& ar)
    {
        for (auto& val : data)
        {
            ar & val;
        }
    }

    bool operator==(const LargeMessage& other) const { return data == other.data; }
};

class SerializationMuxTest : public ::testing::Test
{
protected:
    void SetUp() override { buffer = std::make_unique<LinearBuffer>(1024); }

    void TearDown() override { buffer.reset(); }

    std::unique_ptr<LinearBuffer> buffer;

    std::size_t available_data() const
    {
        std::size_t size = 0;
        buffer->get_read_head(size);
        return size;
    }

    // Helper to read raw bytes from buffer without consuming
    std::vector<std::uint8_t> peek_bytes(std::size_t count) const
    {
        std::size_t available = 0;
        const std::uint8_t* read_head = buffer->get_read_head(available);
        if (read_head == nullptr || available < count)
        {
            return {};
        }
        return std::vector<std::uint8_t>(read_head, read_head + count);
    }
};

// Test basic serialization with single type
TEST_F(SerializationMuxTest, SerializeSingleType)
{
    MultiplexingSerializer<MessageA> mux(*buffer);

    MessageA msg {42, 100};
    ASSERT_TRUE(mux.serialize(msg));

    // Verify data was written
    EXPECT_GT(available_data(), 0);
}

// Test serialization with multiple types
TEST_F(SerializationMuxTest, SerializeMultipleTypes)
{
    MultiplexingSerializer<MessageA, MessageB, MessageC> mux(*buffer);

    MessageA msg_a {1, 10};
    MessageB msg_b {5, -42, true};
    MessageC msg_c {1234567890};

    ASSERT_TRUE(mux.serialize(msg_a));
    ASSERT_TRUE(mux.serialize(msg_b));
    ASSERT_TRUE(mux.serialize(msg_c));

    EXPECT_GT(available_data(), 0);
}

// Test header format (type_index + size)
TEST_F(SerializationMuxTest, HeaderFormat)
{
    MultiplexingSerializer<MessageA, MessageB> mux(*buffer);

    MessageA msg {42, 100};
    ASSERT_TRUE(mux.serialize(msg));

    // Read header bytes
    auto header = peek_bytes(3);
    ASSERT_EQ(header.size(), 3);

    // Type index should be 0 for MessageA (first in list)
    EXPECT_EQ(header[0], 0);

    // Size should be 6 bytes (uint32_t + uint16_t)
    std::uint16_t size = static_cast<std::uint16_t>(header[1]) | (static_cast<std::uint16_t>(header[2]) << 8);
    EXPECT_EQ(size, 6);
}

// Test type indices are correct
TEST_F(SerializationMuxTest, TypeIndices)
{
    MultiplexingSerializer<MessageA, MessageB, MessageC> mux(*buffer);

    // Serialize MessageB (index 1)
    MessageB msg {5, -42, true};
    ASSERT_TRUE(mux.serialize(msg));

    auto header = peek_bytes(3);
    ASSERT_EQ(header.size(), 3);
    EXPECT_EQ(header[0], 1); // MessageB is second in type list
}

// Test empty message serialization
TEST_F(SerializationMuxTest, EmptyMessage)
{
    MultiplexingSerializer<EmptyMessage> mux(*buffer);

    EmptyMessage msg;
    ASSERT_TRUE(mux.serialize(msg));

    auto header = peek_bytes(3);
    ASSERT_EQ(header.size(), 3);

    // Type index 0
    EXPECT_EQ(header[0], 0);

    // Size should be 0
    std::uint16_t size = static_cast<std::uint16_t>(header[1]) | (static_cast<std::uint16_t>(header[2]) << 8);
    EXPECT_EQ(size, 0);
}

// Test deserialization with single type
TEST_F(SerializationMuxTest, DeserializeSingleType)
{
    MultiplexingSerializer<MessageA> mux(*buffer);
    DemultiplexingDeserializer<MessageA> demux(*buffer);

    MessageA original {42, 100};
    ASSERT_TRUE(mux.serialize(original));

    bool success = false;
    auto variant = demux.deserialize(success);
    ASSERT_TRUE(success);

    ASSERT_TRUE(std::holds_alternative<MessageA>(variant));
    MessageA deserialized = std::get<MessageA>(variant);
    EXPECT_EQ(deserialized, original);
}

// Test deserialization with multiple types
TEST_F(SerializationMuxTest, DeserializeMultipleTypes)
{
    MultiplexingSerializer<MessageA, MessageB, MessageC> mux(*buffer);
    DemultiplexingDeserializer<MessageA, MessageB, MessageC> demux(*buffer);

    MessageA msg_a {1, 10};
    MessageB msg_b {5, -42, true};
    MessageC msg_c {1234567890};

    ASSERT_TRUE(mux.serialize(msg_a));
    ASSERT_TRUE(mux.serialize(msg_b));
    ASSERT_TRUE(mux.serialize(msg_c));

    // Deserialize first message (MessageA)
    {
        bool success = false;
        auto variant = demux.deserialize(success);
        ASSERT_TRUE(success);
        ASSERT_TRUE(std::holds_alternative<MessageA>(variant));
        EXPECT_EQ(std::get<MessageA>(variant), msg_a);
    }

    // Deserialize second message (MessageB)
    {
        bool success = false;
        auto variant = demux.deserialize(success);
        ASSERT_TRUE(success);
        ASSERT_TRUE(std::holds_alternative<MessageB>(variant));
        EXPECT_EQ(std::get<MessageB>(variant), msg_b);
    }

    // Deserialize third message (MessageC)
    {
        bool success = false;
        auto variant = demux.deserialize(success);
        ASSERT_TRUE(success);
        ASSERT_TRUE(std::holds_alternative<MessageC>(variant));
        EXPECT_EQ(std::get<MessageC>(variant), msg_c);
    }
}

// Test round-trip with all types
TEST_F(SerializationMuxTest, RoundTripAllTypes)
{
    MultiplexingSerializer<MessageA, MessageB, MessageC> mux(*buffer);
    DemultiplexingDeserializer<MessageA, MessageB, MessageC> demux(*buffer);

    // Serialize in mixed order
    MessageC msg_c {999};
    MessageA msg_a {123, 456};
    MessageB msg_b {7, 100, false};

    ASSERT_TRUE(mux.serialize(msg_c));
    ASSERT_TRUE(mux.serialize(msg_a));
    ASSERT_TRUE(mux.serialize(msg_b));

    // Deserialize and verify order preserved
    {
        bool success = false;
        auto variant = demux.deserialize(success);
        ASSERT_TRUE(success);
        ASSERT_TRUE(std::holds_alternative<MessageC>(variant));
        EXPECT_EQ(std::get<MessageC>(variant), msg_c);
    }

    {
        bool success = false;
        auto variant = demux.deserialize(success);
        ASSERT_TRUE(success);
        ASSERT_TRUE(std::holds_alternative<MessageA>(variant));
        EXPECT_EQ(std::get<MessageA>(variant), msg_a);
    }

    {
        bool success = false;
        auto variant = demux.deserialize(success);
        ASSERT_TRUE(success);
        ASSERT_TRUE(std::holds_alternative<MessageB>(variant));
        EXPECT_EQ(std::get<MessageB>(variant), msg_b);
    }
}

// Test deserialization with empty buffer
TEST_F(SerializationMuxTest, DeserializeEmptyBuffer)
{
    DemultiplexingDeserializer<MessageA> demux(*buffer);

    bool success = true;
    auto variant = demux.deserialize(success);
    EXPECT_FALSE(success);
}

// Test deserialization with incomplete header
TEST_F(SerializationMuxTest, DeserializeIncompleteHeader)
{
    // Manually write only 2 bytes (incomplete header)
    std::size_t available = 0;
    std::uint8_t* write_head = buffer->get_write_head(available);
    ASSERT_NE(write_head, nullptr);
    write_head[0] = 0;
    write_head[1] = 5;
    ASSERT_TRUE(buffer->commit(2));

    DemultiplexingDeserializer<MessageA> demux(*buffer);
    bool success = true;
    auto variant = demux.deserialize(success);
    EXPECT_FALSE(success);
}

// Test deserialization with invalid type index
TEST_F(SerializationMuxTest, DeserializeInvalidTypeIndex)
{
    // Manually write header with invalid type index
    std::size_t available = 0;
    std::uint8_t* write_head = buffer->get_write_head(available);
    ASSERT_NE(write_head, nullptr);
    write_head[0] = 5; // Invalid index (only 2 types available)
    write_head[1] = 0;
    write_head[2] = 0;
    ASSERT_TRUE(buffer->commit(3));

    DemultiplexingDeserializer<MessageA, MessageB> demux(*buffer);
    bool success = true;
    auto variant = demux.deserialize(success);
    EXPECT_FALSE(success);
}

// Test buffer exhaustion during serialization
TEST_F(SerializationMuxTest, BufferExhaustion)
{
    LinearBuffer small_buffer(10); // Very small buffer
    MultiplexingSerializer<MessageA> mux(small_buffer);

    MessageA msg {42, 100};
    // Header is 3 bytes, payload is 6 bytes, total 9 bytes - should fit
    ASSERT_TRUE(mux.serialize(msg));

    // Second message won't fit
    MessageA msg2 {99, 200};
    EXPECT_FALSE(mux.serialize(msg2));
}

// Test multiple messages in same buffer
TEST_F(SerializationMuxTest, MultipleMessagesInBuffer)
{
    MultiplexingSerializer<MessageA, MessageB> mux(*buffer);
    DemultiplexingDeserializer<MessageA, MessageB> demux(*buffer);

    // Serialize 5 messages of mixed types
    MessageA msg_a1 {1, 10};
    MessageB msg_b1 {2, 20, true};
    MessageA msg_a2 {3, 30};
    MessageA msg_a3 {4, 40};
    MessageB msg_b2 {5, 50, false};

    ASSERT_TRUE(mux.serialize(msg_a1));
    ASSERT_TRUE(mux.serialize(msg_b1));
    ASSERT_TRUE(mux.serialize(msg_a2));
    ASSERT_TRUE(mux.serialize(msg_a3));
    ASSERT_TRUE(mux.serialize(msg_b2));

    // Deserialize all and verify
    std::vector<std::pair<std::string, bool>> results;

    for (int i = 0; i < 5; ++i)
    {
        bool success = false;
        auto variant = demux.deserialize(success);
        ASSERT_TRUE(success) << "Failed at message " << i;

        std::visit(
            [&](auto&& msg)
            {
                using T = std::decay_t<decltype(msg)>;
                if constexpr (std::is_same_v<T, MessageA>)
                {
                    results.push_back({"A", true});
                }
                else if constexpr (std::is_same_v<T, MessageB>)
                {
                    results.push_back({"B", true});
                }
            },
            variant);
    }

    ASSERT_EQ(results.size(), 5);
    EXPECT_EQ(results[0].first, "A");
    EXPECT_EQ(results[1].first, "B");
    EXPECT_EQ(results[2].first, "A");
    EXPECT_EQ(results[3].first, "A");
    EXPECT_EQ(results[4].first, "B");
}

// Test empty message deserialization
TEST_F(SerializationMuxTest, EmptyMessageRoundTrip)
{
    MultiplexingSerializer<EmptyMessage> mux(*buffer);
    DemultiplexingDeserializer<EmptyMessage> demux(*buffer);

    EmptyMessage msg;
    ASSERT_TRUE(mux.serialize(msg));

    bool success = false;
    auto variant = demux.deserialize(success);
    ASSERT_TRUE(success);
    ASSERT_TRUE(std::holds_alternative<EmptyMessage>(variant));
}

// Test large message (near size limit)
TEST_F(SerializationMuxTest, LargeMessage)
{
    LinearBuffer large_buffer(1024);
    MultiplexingSerializer<LargeMessage> mux(large_buffer);
    DemultiplexingDeserializer<LargeMessage> demux(large_buffer);

    LargeMessage msg;
    for (std::size_t i = 0; i < msg.data.size(); ++i)
    {
        msg.data[i] = i * 123;
    }

    ASSERT_TRUE(mux.serialize(msg));

    bool success = false;
    auto variant = demux.deserialize(success);
    ASSERT_TRUE(success);
    ASSERT_TRUE(std::holds_alternative<LargeMessage>(variant));
    EXPECT_EQ(std::get<LargeMessage>(variant), msg);
}

// Test serialization preserves binary representation
TEST_F(SerializationMuxTest, BinaryRepresentation)
{
    MultiplexingSerializer<MessageA> mux(*buffer);

    MessageA msg {0x12345678, 0xABCD};
    ASSERT_TRUE(mux.serialize(msg));

    // Read raw bytes (skip header)
    std::size_t available = 0;
    const std::uint8_t* read_head = buffer->get_read_head(available);
    ASSERT_GE(available, 9); // 3 header + 6 payload

    // Check payload bytes (little-endian)
    EXPECT_EQ(read_head[3], 0x78); // id low byte
    EXPECT_EQ(read_head[4], 0x56);
    EXPECT_EQ(read_head[5], 0x34);
    EXPECT_EQ(read_head[6], 0x12); // id high byte
    EXPECT_EQ(read_head[7], 0xCD); // value low byte
    EXPECT_EQ(read_head[8], 0xAB); // value high byte
}

// Test deserialization fails with incomplete payload
TEST_F(SerializationMuxTest, IncompletePayloadValidation)
{
    // Write header claiming 6 bytes but provide no payload
    std::size_t available = 0;
    std::uint8_t* write_head = buffer->get_write_head(available);
    ASSERT_NE(write_head, nullptr);
    write_head[0] = 0;    // type index 0 (MessageA)
    write_head[1] = 0x06; // size low byte (6 bytes claimed)
    write_head[2] = 0x00; // size high byte
    ASSERT_TRUE(buffer->commit(3));

    DemultiplexingDeserializer<MessageA> demux(*buffer);
    bool success = true; // Start with true to verify it gets set to false
    auto variant = demux.deserialize(success);

    // Should fail because payload is incomplete
    EXPECT_FALSE(success);

    // Header should have been consumed
    EXPECT_EQ(available_data(), 0);
}

// Test variant correctly identifies types
TEST_F(SerializationMuxTest, VariantTypeIdentification)
{
    MultiplexingSerializer<MessageA, MessageB, MessageC> mux(*buffer);
    DemultiplexingDeserializer<MessageA, MessageB, MessageC> demux(*buffer);

    MessageB msg {99, -1000, true};
    ASSERT_TRUE(mux.serialize(msg));

    bool success = false;
    auto variant = demux.deserialize(success);
    ASSERT_TRUE(success);

    // Check variant holds correct type
    EXPECT_FALSE(std::holds_alternative<MessageA>(variant));
    EXPECT_TRUE(std::holds_alternative<MessageB>(variant));
    EXPECT_FALSE(std::holds_alternative<MessageC>(variant));

    // Verify data
    MessageB deserialized = std::get<MessageB>(variant);
    EXPECT_EQ(deserialized, msg);
}

} // namespace eestv
