#include <gtest/gtest.h>

#include <vector>

#include "shift.hpp"
#include "daily_set.hpp"
#include "weekend_shifts.hpp"

namespace roster_slayer
{

namespace
{
std::vector<Shift> make_desired_shifts(std::size_t count)
{
    return std::vector<Shift>(count, Shift{"EA", StartTime{Hour{7}, Minute{0}}, StopTime{Hour{15}, Minute{15}}, 7.5});
}
}

TEST(DailySetTest, ConstructWithTenRows)
{
    EXPECT_NO_THROW(DailySet{make_desired_shifts(10)});
}

TEST(DailySetTest, ConstructWithCustomSize)
{
    EXPECT_NO_THROW(DailySet{make_desired_shifts(4)});
}

TEST(DailySetTest, ConstructWithZeroRows)
{
    EXPECT_NO_THROW(DailySet{make_desired_shifts(0)});
}

TEST(DailySetTest, ConstructWithCustomSizeThirtyTwo)
{
    EXPECT_NO_THROW(DailySet{make_desired_shifts(42)});
}

TEST(DailySetTest, StoresShiftAtValidIndex)
{
    DailySet daily_set{make_desired_shifts(4)};
    const auto shift = Shift("EA", StartTime{Hour{7}, Minute{0}}, StopTime{Hour{15}, Minute{15}}, 7.5);

    daily_set.set(2, shift);

    EXPECT_EQ(daily_set.get(2).get_code(), "EA");
    EXPECT_EQ(daily_set.size(), 4u);
}

TEST(DailySetTest, RejectsOutOfRangeIndexForSet)
{
    DailySet daily_set{make_desired_shifts(4)};
    const auto shift = Shift("OFF", StartTime{Hour{0}, Minute{0}}, StopTime{Hour{0}, Minute{0}}, 0.0);

    EXPECT_THROW(daily_set.set(4, shift), std::out_of_range);
}

TEST(DailySetTest, RejectsOutOfRangeIndexForGet)
{
    const DailySet daily_set{make_desired_shifts(4)};

    EXPECT_THROW((void)daily_set.get(4), std::out_of_range);
}

TEST(DailySetTest, ReturnsRemainingCombinationCountForPartiallyFilledWeekendSet)
{
    const auto required_shifts = weekend_shifts::get_weekly_required_shifts();
    DailySet set{required_shifts};

    // Required weekend shifts:
    // OFF x5, WE-EA x1, WE-EB x1, WE-SD x1, WE-SL x1, WE-L x1
    //
    // Total combinations:
    // 10! / 5! = 30240
    EXPECT_EQ(set.get_nr_of_combinations(), 30240u);

    // Fill one OFF:
    // remaining = 9! / 4! = 15120
    set.set(0, weekend_shifts::get_off_shift());
    EXPECT_EQ(set.get_nr_of_combinations(), 15120u);

    // Fill one WE-L:
    // remaining = 8! / 4! = 1680
    set.set(1, weekend_shifts::get_weekend_late_shift());
    EXPECT_EQ(set.get_nr_of_combinations(), 1680u);
}

} // namespace roster_slayer
