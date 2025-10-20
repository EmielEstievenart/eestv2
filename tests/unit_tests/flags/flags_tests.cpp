#include <gtest/gtest.h>
#include "eestv/flags/flags.hpp"

using namespace eestv;

enum class TestFlags
{
    Flag0  = 0,
    Flag1  = 1,
    Flag2  = 2,
    Flag31 = 31
};

TEST(FlagsTest, InitialStateIsAllClear)
{
    Flags<TestFlags> flags;
    EXPECT_FALSE(flags.get_flag(TestFlags::Flag0));
    EXPECT_FALSE(flags.get_flag(TestFlags::Flag1));
    EXPECT_EQ(flags.get_raw(), 0U);
}

TEST(FlagsTest, SetAndGetSingleFlag)
{
    Flags<TestFlags> flags;
    flags.set_flag(TestFlags::Flag1);

    EXPECT_TRUE(flags.get_flag(TestFlags::Flag1));
    EXPECT_FALSE(flags.get_flag(TestFlags::Flag0));
    EXPECT_EQ(flags.get_raw(), 0x02U);
}

TEST(FlagsTest, SetMultipleFlags)
{
    Flags<TestFlags> flags;
    flags.set_flag(TestFlags::Flag0);
    flags.set_flag(TestFlags::Flag2);

    EXPECT_TRUE(flags.get_flag(TestFlags::Flag0));
    EXPECT_TRUE(flags.get_flag(TestFlags::Flag2));
    EXPECT_FALSE(flags.get_flag(TestFlags::Flag1));
    EXPECT_EQ(flags.get_raw(), 0x05U);
}

TEST(FlagsTest, ClearFlag)
{
    Flags<TestFlags> flags;
    flags.set_flag(TestFlags::Flag1);
    flags.set_flag(TestFlags::Flag2);
    
    flags.clear_flag(TestFlags::Flag1);

    EXPECT_FALSE(flags.get_flag(TestFlags::Flag1));
    EXPECT_TRUE(flags.get_flag(TestFlags::Flag2));
}

TEST(FlagsTest, ClearAll)
{
    Flags<TestFlags> flags;
    flags.set_flag(TestFlags::Flag0);
    flags.set_flag(TestFlags::Flag1);
    flags.set_flag(TestFlags::Flag2);

    flags.clear_all();

    EXPECT_EQ(flags.get_raw(), 0U);
    EXPECT_FALSE(flags.get_flag(TestFlags::Flag0));
    EXPECT_FALSE(flags.get_flag(TestFlags::Flag1));
    EXPECT_FALSE(flags.get_flag(TestFlags::Flag2));
}

TEST(FlagsTest, MaxBitPosition)
{
    Flags<TestFlags> flags;
    flags.set_flag(TestFlags::Flag31);

    EXPECT_TRUE(flags.get_flag(TestFlags::Flag31));
    EXPECT_EQ(flags.get_raw(), 0x80000000U);
}
