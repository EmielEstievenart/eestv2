#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "shift.hpp"
#include "weekday_shifts.hpp"
#include "weekend_shifts.hpp"

namespace roster_slayer
{

TEST(ShiftTest, WeekdayOffShiftIsZeroHoursAndOff)
{
    const auto off_shift = weekday_shifts::get_off_shift();

    EXPECT_EQ(off_shift.get_code(), "OFF");
    EXPECT_DOUBLE_EQ(off_shift.number_of_hours_worked(), 0.0);
    EXPECT_TRUE(off_shift.is_off());
    EXPECT_EQ(off_shift.start_time_to_string(), "00:00");
    EXPECT_EQ(off_shift.end_time_to_string(), "00:00");
}

TEST(ShiftTest, WeekendOffShiftIsZeroHoursAndOff)
{
    const auto off_shift = weekend_shifts::get_off_shift();

    EXPECT_EQ(off_shift.get_code(), "OFF");
    EXPECT_DOUBLE_EQ(off_shift.number_of_hours_worked(), 0.0);
    EXPECT_TRUE(off_shift.is_off());
}

TEST(ShiftTest, NonOffShiftRequiresPositiveHours)
{
    EXPECT_THROW(Shift("X", StartTime{Hour{8}, Minute{0}}, StopTime{Hour{9}, Minute{0}}, 0.0), std::invalid_argument);
}

TEST(ShiftTest, WeekdayRequiredShiftsContainThreeOffSlotsAndFiveWorkingShifts)
{
    const auto shifts = weekday_shifts::get_weekly_required_shifts();

    EXPECT_EQ(shifts.size(), 8u);
    EXPECT_TRUE(std::all_of(shifts.begin(), shifts.begin() + 3, [](const Shift& shift)
                            { return shift.is_off(); }));

    EXPECT_EQ(shifts[3].get_code(), "EA");
    EXPECT_EQ(shifts[4].get_code(), "EB");
    EXPECT_EQ(shifts[5].get_code(), "D");
    EXPECT_EQ(shifts[6].get_code(), "L1");
    EXPECT_EQ(shifts[7].get_code(), "L2");
}

TEST(ShiftTest, WeekendRequiredShiftsContainFiveOffSlotsAndFiveWorkingShifts)
{
    const auto shifts = weekend_shifts::get_weekly_required_shifts();

    EXPECT_EQ(shifts.size(), 10u);
    EXPECT_TRUE(std::all_of(shifts.begin(), shifts.begin() + 5, [](const Shift& shift)
                            { return shift.is_off(); }));

    EXPECT_EQ(shifts[5].get_code(), "WE-EA");
    EXPECT_EQ(shifts[6].get_code(), "WE-EB");
    EXPECT_EQ(shifts[7].get_code(), "WE-SD");
    EXPECT_EQ(shifts[8].get_code(), "WE-SL");
    EXPECT_EQ(shifts[9].get_code(), "WE-L");
}

} // namespace roster_slayer
