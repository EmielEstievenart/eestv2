#include <gtest/gtest.h>
#include "eestv/serial/serialize.hpp"
#include "eestv/data/linear_buffer.hpp"
#include <cstdint>
#include <cstring>

namespace eestv
{

// Test data structures defined at namespace scope
struct TestData
{
    std::uint32_t id;
    std::int16_t temperature;
    bool active;

    template <typename Archive>
    void serialize(Archive& ar)
    {
        ar & id & temperature & active;
    }
};

struct Inner
{
    std::uint16_t x;
    std::uint16_t y;

    template <typename Archive>
    void serialize(Archive& ar)
    {
        ar & x & y;
    }
};

struct Outer
{
    std::uint32_t id;
    Inner position;
    bool active;

    template <typename Archive>
    void serialize(Archive& ar)
    {
        ar & id & position & active;
    }
};

class SerializationTest : public ::testing::Test
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
};

// Test serialization of primitive types
TEST_F(SerializationTest, SerializePrimitiveTypes)
{
    Serializer ser(*buffer);

    std::uint8_t u8   = 0x42;
    std::uint16_t u16 = 0x1234;
    std::uint32_t u32 = 0x12345678;
    std::uint64_t u64 = 0x123456789ABCDEF0;

    std::int8_t i8   = -42;
    std::int16_t i16 = -1234;
    std::int32_t i32 = -123456;
    std::int64_t i64 = -123456789;

    ser & u8 & u16 & u32 & u64 & i8 & i16 & i32 & i64;

    // Verify bytes were written
    std::size_t expected_size = sizeof(u8) + sizeof(u16) + sizeof(u32) + sizeof(u64) + sizeof(i8) + sizeof(i16) + sizeof(i32) + sizeof(i64);
    EXPECT_EQ(ser.bytes_written(), expected_size);
    EXPECT_EQ(available_data(), expected_size);
}

// Test deserialization of primitive types
TEST_F(SerializationTest, DeserializePrimitiveTypes)
{
    // First serialize some data
    {
        Serializer ser(*buffer);

        std::uint8_t u8   = 0x42;
        std::uint16_t u16 = 0x1234;
        std::uint32_t u32 = 0x12345678;

        ser & u8 & u16 & u32;
    }

    // Now deserialize
    {
        Deserializer deser(*buffer);

        std::uint8_t u8   = 0;
        std::uint16_t u16 = 0;
        std::uint32_t u32 = 0;

        deser & u8 & u16 & u32;

        EXPECT_EQ(u8, 0x42);
        EXPECT_EQ(u16, 0x1234);
        EXPECT_EQ(u32, 0x12345678);
        EXPECT_EQ(deser.bytes_read(), sizeof(u8) + sizeof(u16) + sizeof(u32));
    }

    // Buffer should be empty after deserialization
    EXPECT_EQ(available_data(), 0);
}

// Test serialization and deserialization of bool
TEST_F(SerializationTest, SerializeDeserializeBool)
{
    // Serialize
    {
        Serializer ser(*buffer);
        bool flag_true  = true;
        bool flag_false = false;

        ser & flag_true & flag_false;

        EXPECT_EQ(ser.bytes_written(), 2 * sizeof(bool));
    }

    // Deserialize
    {
        Deserializer deser(*buffer);
        bool flag1 = false;
        bool flag2 = true;

        deser & flag1 & flag2;

        EXPECT_TRUE(flag1);
        EXPECT_FALSE(flag2);
    }
}

// Test serialization with user-defined struct
TEST_F(SerializationTest, SerializeUserDefinedStruct)
{

    // Serialize
    {
        Serializer ser(*buffer);
        TestData data {42, -15, true};

        ser & data;

        std::size_t expected_size = sizeof(data.id) + sizeof(data.temperature) + sizeof(data.active);
        EXPECT_EQ(ser.bytes_written(), expected_size);
    }

    // Deserialize
    {
        Deserializer deser(*buffer);
        TestData data {0, 0, false};

        deser & data;

        EXPECT_EQ(data.id, 42);
        EXPECT_EQ(data.temperature, -15);
        EXPECT_TRUE(data.active);
    }
}

// Test chaining multiple values
TEST_F(SerializationTest, ChainingOperator)
{
    std::uint8_t a  = 10;
    std::uint16_t b = 20;
    std::uint32_t c = 30;

    // Serialize with chaining
    {
        Serializer ser(*buffer);
        ser & a & b & c;

        EXPECT_EQ(ser.bytes_written(), sizeof(a) + sizeof(b) + sizeof(c));
    }

    // Deserialize with chaining
    {
        Deserializer deser(*buffer);
        std::uint8_t a_out  = 0;
        std::uint16_t b_out = 0;
        std::uint32_t c_out = 0;

        deser & a_out & b_out & c_out;

        EXPECT_EQ(a_out, a);
        EXPECT_EQ(b_out, b);
        EXPECT_EQ(c_out, c);
    }
}

// Test serializer reset functionality
TEST_F(SerializationTest, SerializerReset)
{
    Serializer ser(*buffer);

    std::uint32_t value = 0x12345678;
    ser & value;

    EXPECT_EQ(ser.bytes_written(), sizeof(value));

    ser.reset();
    EXPECT_EQ(ser.bytes_written(), 0);
}

// Test deserializer reset functionality
TEST_F(SerializationTest, DeserializerReset)
{
    // First serialize
    {
        Serializer ser(*buffer);
        std::uint32_t value = 0x12345678;
        ser & value;
    }

    // Deserialize and reset
    Deserializer deser(*buffer);
    std::uint32_t value = 0;
    deser & value;

    EXPECT_EQ(deser.bytes_read(), sizeof(value));

    deser.reset();
    EXPECT_EQ(deser.bytes_read(), 0);
}

// Test insufficient buffer space
TEST_F(SerializationTest, InsufficientBufferSpace)
{
    // Create a small buffer
    LinearBuffer small_buffer(4);
    Serializer ser(small_buffer);

    std::uint32_t value1 = 0x12345678;
    std::uint32_t value2 = 0x87654321;

    // First write should succeed
    ser & value1;
    EXPECT_EQ(ser.bytes_written(), sizeof(value1));

    // Second write should fail (no space)
    ser & value2;
    // bytes_written should not increase if write fails
    EXPECT_EQ(ser.bytes_written(), sizeof(value1));
}

// Test insufficient data for deserialization
TEST_F(SerializationTest, InsufficientDataForDeserialization)
{
    // Serialize only one value
    {
        Serializer ser(*buffer);
        std::uint32_t value = 0x12345678;
        ser & value;
    }

    // Try to deserialize two values
    Deserializer deser(*buffer);
    std::uint32_t value1 = 0;
    std::uint32_t value2 = 0;

    deser & value1; // Should succeed
    EXPECT_EQ(value1, 0x12345678);
    EXPECT_EQ(deser.bytes_read(), sizeof(value1));

    deser & value2; // Should fail (no data)
    // bytes_read should not increase if read fails
    EXPECT_EQ(deser.bytes_read(), sizeof(value1));
}

// Test nested structs
TEST_F(SerializationTest, NestedStructs)
{

    // Serialize
    {
        Serializer ser(*buffer);
        Outer data {100, {50, 75}, true};
        ser & data;
    }

    // Deserialize
    {
        Deserializer deser(*buffer);
        Outer data {0, {0, 0}, false};
        deser & data;

        EXPECT_EQ(data.id, 100);
        EXPECT_EQ(data.position.x, 50);
        EXPECT_EQ(data.position.y, 75);
        EXPECT_TRUE(data.active);
    }
}

} // namespace eestv::serial
