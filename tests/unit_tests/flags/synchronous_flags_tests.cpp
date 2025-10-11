#include <gtest/gtest.h>
#include "eestv/flags/synchronous_flags.hpp"
#include <thread>
#include <vector>
#include <atomic>

using namespace eestv;

// Define test enum classes
enum class TestFlags
{
    Flag0 = 0,
    Flag1 = 1,
    Flag2 = 2,
    Flag3 = 3,
    Flag4 = 4,
    Flag31 = 31 // Maximum for 32-bit uint
};

enum class StatusFlags
{
    Ready = 0,
    Running = 1,
    Paused = 2,
    Error = 3
};

class SynchronousFlagsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        flags = std::make_unique<SynchronousFlags<TestFlags>>();
    }

    void TearDown() override
    {
        flags.reset();
    }

    std::unique_ptr<SynchronousFlags<TestFlags>> flags;
};

// Basic functionality tests
TEST_F(SynchronousFlagsTest, InitialStateIsAllClear)
{
    EXPECT_FALSE(flags->get_flag(TestFlags::Flag0));
    EXPECT_FALSE(flags->get_flag(TestFlags::Flag1));
    EXPECT_FALSE(flags->get_flag(TestFlags::Flag2));
    EXPECT_EQ(flags->get_raw(), 0U);
}

TEST_F(SynchronousFlagsTest, SetSingleFlag)
{
    flags->set_flag(TestFlags::Flag1);
    
    EXPECT_FALSE(flags->get_flag(TestFlags::Flag0));
    EXPECT_TRUE(flags->get_flag(TestFlags::Flag1));
    EXPECT_FALSE(flags->get_flag(TestFlags::Flag2));
    EXPECT_EQ(flags->get_raw(), 0x02U); // Bit 1 set
}

TEST_F(SynchronousFlagsTest, SetMultipleFlags)
{
    flags->set_flag(TestFlags::Flag0);
    flags->set_flag(TestFlags::Flag2);
    flags->set_flag(TestFlags::Flag4);
    
    EXPECT_TRUE(flags->get_flag(TestFlags::Flag0));
    EXPECT_FALSE(flags->get_flag(TestFlags::Flag1));
    EXPECT_TRUE(flags->get_flag(TestFlags::Flag2));
    EXPECT_FALSE(flags->get_flag(TestFlags::Flag3));
    EXPECT_TRUE(flags->get_flag(TestFlags::Flag4));
    EXPECT_EQ(flags->get_raw(), 0x15U); // Bits 0, 2, 4 set (0x01 | 0x04 | 0x10)
}

TEST_F(SynchronousFlagsTest, ClearSingleFlag)
{
    flags->set_flag(TestFlags::Flag1);
    flags->set_flag(TestFlags::Flag2);
    
    EXPECT_TRUE(flags->get_flag(TestFlags::Flag1));
    EXPECT_TRUE(flags->get_flag(TestFlags::Flag2));
    
    flags->clear_flag(TestFlags::Flag1);
    
    EXPECT_FALSE(flags->get_flag(TestFlags::Flag1));
    EXPECT_TRUE(flags->get_flag(TestFlags::Flag2));
    EXPECT_EQ(flags->get_raw(), 0x04U); // Only bit 2 set
}

TEST_F(SynchronousFlagsTest, ClearFlagThatIsNotSet)
{
    flags->set_flag(TestFlags::Flag1);
    
    flags->clear_flag(TestFlags::Flag2); // Clear flag that wasn't set
    
    EXPECT_TRUE(flags->get_flag(TestFlags::Flag1));
    EXPECT_FALSE(flags->get_flag(TestFlags::Flag2));
    EXPECT_EQ(flags->get_raw(), 0x02U);
}

TEST_F(SynchronousFlagsTest, SetSameFlagMultipleTimes)
{
    flags->set_flag(TestFlags::Flag1);
    flags->set_flag(TestFlags::Flag1);
    flags->set_flag(TestFlags::Flag1);
    
    EXPECT_TRUE(flags->get_flag(TestFlags::Flag1));
    EXPECT_EQ(flags->get_raw(), 0x02U);
}

TEST_F(SynchronousFlagsTest, ClearAllFlags)
{
    flags->set_flag(TestFlags::Flag0);
    flags->set_flag(TestFlags::Flag1);
    flags->set_flag(TestFlags::Flag2);
    flags->set_flag(TestFlags::Flag3);
    
    EXPECT_NE(flags->get_raw(), 0U);
    
    flags->clear_all();
    
    EXPECT_FALSE(flags->get_flag(TestFlags::Flag0));
    EXPECT_FALSE(flags->get_flag(TestFlags::Flag1));
    EXPECT_FALSE(flags->get_flag(TestFlags::Flag2));
    EXPECT_FALSE(flags->get_flag(TestFlags::Flag3));
    EXPECT_EQ(flags->get_raw(), 0U);
}

TEST_F(SynchronousFlagsTest, ToggleFlagOnAndOff)
{
    flags->set_flag(TestFlags::Flag2);
    EXPECT_TRUE(flags->get_flag(TestFlags::Flag2));
    
    flags->clear_flag(TestFlags::Flag2);
    EXPECT_FALSE(flags->get_flag(TestFlags::Flag2));
    
    flags->set_flag(TestFlags::Flag2);
    EXPECT_TRUE(flags->get_flag(TestFlags::Flag2));
}

TEST_F(SynchronousFlagsTest, MaxBitPosition)
{
    flags->set_flag(TestFlags::Flag31);
    
    EXPECT_TRUE(flags->get_flag(TestFlags::Flag31));
    EXPECT_EQ(flags->get_raw(), 0x80000000U); // Bit 31 set
}

TEST_F(SynchronousFlagsTest, AllBitsCombination)
{
    flags->set_flag(TestFlags::Flag0);
    flags->set_flag(TestFlags::Flag31);
    
    EXPECT_TRUE(flags->get_flag(TestFlags::Flag0));
    EXPECT_TRUE(flags->get_flag(TestFlags::Flag31));
    EXPECT_EQ(flags->get_raw(), 0x80000001U);
}

// Multi-threading tests
TEST_F(SynchronousFlagsTest, ConcurrentSetOperations)
{
    const int num_threads = 8;
    std::vector<std::thread> threads;
    
    // Each thread sets a different flag
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([this, i]()
        {
            TestFlags flag = static_cast<TestFlags>(i);
            for (int j = 0; j < 1000; ++j)
            {
                flags->set_flag(flag);
            }
        });
    }
    
    for (auto& thread : threads)
    {
        thread.join();
    }
    
    // Verify all flags are set
    for (int i = 0; i < num_threads; ++i)
    {
        TestFlags flag = static_cast<TestFlags>(i);
        EXPECT_TRUE(flags->get_flag(flag));
    }
}

TEST_F(SynchronousFlagsTest, ConcurrentSetAndClearOperations)
{
    const int num_threads = 4;
    const int iterations = 1000;
    std::vector<std::thread> threads;
    
    // Half threads set, half threads clear the same flag
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([this, i, iterations]()
        {
            for (int j = 0; j < iterations; ++j)
            {
                if (i % 2 == 0)
                {
                    flags->set_flag(TestFlags::Flag1);
                }
                else
                {
                    flags->clear_flag(TestFlags::Flag1);
                }
            }
        });
    }
    
    for (auto& thread : threads)
    {
        thread.join();
    }
    
    // Flag should be in a valid state (either set or clear)
    bool flag_state = flags->get_flag(TestFlags::Flag1);
    uint32_t raw = flags->get_raw();
    
    if (flag_state)
    {
        EXPECT_EQ(raw & 0x02U, 0x02U);
    }
    else
    {
        EXPECT_EQ(raw & 0x02U, 0x00U);
    }
}

TEST_F(SynchronousFlagsTest, ConcurrentReadOperations)
{
    flags->set_flag(TestFlags::Flag0);
    flags->set_flag(TestFlags::Flag1);
    flags->set_flag(TestFlags::Flag2);
    
    const int num_threads = 10;
    const int iterations = 1000;
    std::vector<std::thread> threads;
    std::atomic<int> successful_reads{0};
    
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([this, iterations, &successful_reads]()
        {
            for (int j = 0; j < iterations; ++j)
            {
                bool f0 = flags->get_flag(TestFlags::Flag0);
                bool f1 = flags->get_flag(TestFlags::Flag1);
                bool f2 = flags->get_flag(TestFlags::Flag2);
                
                if (f0 && f1 && f2)
                {
                    successful_reads++;
                }
            }
        });
    }
    
    for (auto& thread : threads)
    {
        thread.join();
    }
    
    // All reads should be successful since flags don't change
    EXPECT_EQ(successful_reads.load(), num_threads * iterations);
}

TEST_F(SynchronousFlagsTest, ConcurrentMixedOperations)
{
    const int num_threads = 8;
    const int iterations = 500;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([this, i, iterations]()
        {
            TestFlags flag = static_cast<TestFlags>(i % 4);
            
            for (int j = 0; j < iterations; ++j)
            {
                if (j % 3 == 0)
                {
                    flags->set_flag(flag);
                }
                else if (j % 3 == 1)
                {
                    flags->get_flag(flag);
                }
                else
                {
                    flags->clear_flag(flag);
                }
            }
        });
    }
    
    for (auto& thread : threads)
    {
        thread.join();
    }
    
    // Just verify we can read the final state without crashes
    uint32_t final_state = flags->get_raw();
    EXPECT_LE(final_state, 0xFFFFFFFFU);
}

TEST_F(SynchronousFlagsTest, ConcurrentClearAll)
{
    flags->set_flag(TestFlags::Flag0);
    flags->set_flag(TestFlags::Flag1);
    flags->set_flag(TestFlags::Flag2);
    
    const int num_threads = 5;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([this]()
        {
            for (int j = 0; j < 100; ++j)
            {
                flags->clear_all();
            }
        });
    }
    
    for (auto& thread : threads)
    {
        thread.join();
    }
    
    EXPECT_EQ(flags->get_raw(), 0U);
    EXPECT_FALSE(flags->get_flag(TestFlags::Flag0));
    EXPECT_FALSE(flags->get_flag(TestFlags::Flag1));
    EXPECT_FALSE(flags->get_flag(TestFlags::Flag2));
}

// Test with different enum class
TEST(SynchronousFlagsStatusTest, DifferentEnumClass)
{
    SynchronousFlags<StatusFlags> status;
    
    status.set_flag(StatusFlags::Ready);
    status.set_flag(StatusFlags::Running);
    
    EXPECT_TRUE(status.get_flag(StatusFlags::Ready));
    EXPECT_TRUE(status.get_flag(StatusFlags::Running));
    EXPECT_FALSE(status.get_flag(StatusFlags::Paused));
    EXPECT_FALSE(status.get_flag(StatusFlags::Error));
    
    status.clear_flag(StatusFlags::Ready);
    status.set_flag(StatusFlags::Error);
    
    EXPECT_FALSE(status.get_flag(StatusFlags::Ready));
    EXPECT_TRUE(status.get_flag(StatusFlags::Running));
    EXPECT_FALSE(status.get_flag(StatusFlags::Paused));
    EXPECT_TRUE(status.get_flag(StatusFlags::Error));
}

// Edge case tests
TEST(SynchronousFlagsEdgeCaseTest, DefaultConstruction)
{
    SynchronousFlags<TestFlags> flags;
    EXPECT_EQ(flags.get_raw(), 0U);
}

TEST(SynchronousFlagsEdgeCaseTest, ConstGetFlag)
{
    SynchronousFlags<TestFlags> flags;
    flags.set_flag(TestFlags::Flag1);
    
    const auto& const_flags = flags;
    EXPECT_TRUE(const_flags.get_flag(TestFlags::Flag1));
    EXPECT_EQ(const_flags.get_raw(), 0x02U);
}
