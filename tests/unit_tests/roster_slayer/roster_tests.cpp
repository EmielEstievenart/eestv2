#include <gtest/gtest.h>

#include <vector>

#include "daily_set.hpp"
#include "roster_validation.hpp"
#include "shift.hpp"
#include "weekday_shifts.hpp"
#include "weekend_shifts.hpp"

namespace roster_slayer
{

namespace
{
std::vector<Shift> make_desired_shifts(std::size_t count)
{
    return std::vector<Shift>(count, Shift {"EA", StartTime {Hour {7}, Minute {0}}, StopTime {Hour {15}, Minute {15}}, 7.5});
}
}

TEST(DailySetTest, ConstructWithTenRows)
{
    EXPECT_NO_THROW(DailySet {make_desired_shifts(10)});
}

TEST(DailySetTest, ConstructWithCustomSize)
{
    EXPECT_NO_THROW(DailySet {make_desired_shifts(4)});
}

TEST(DailySetTest, ConstructWithZeroRows)
{
    EXPECT_NO_THROW(DailySet {make_desired_shifts(0)});
}

TEST(DailySetTest, ConstructWithCustomSizeThirtyTwo)
{
    EXPECT_NO_THROW(DailySet {make_desired_shifts(42)});
}

TEST(DailySetTest, StoresShiftAtValidIndex)
{
    DailySet daily_set {make_desired_shifts(4)};
    const auto shift = Shift("EA", StartTime {Hour {7}, Minute {0}}, StopTime {Hour {15}, Minute {15}}, 7.5);

    daily_set.set(2, shift);

    EXPECT_EQ(daily_set.get(2).get_code(), "EA");
    EXPECT_EQ(daily_set.size(), 4u);
}

TEST(DailySetTest, RejectsOutOfRangeIndexForSet)
{
    DailySet daily_set {make_desired_shifts(4)};
    const auto shift = Shift("OFF", StartTime {Hour {0}, Minute {0}}, StopTime {Hour {0}, Minute {0}}, 0.0);

    EXPECT_THROW(daily_set.set(4, shift), std::out_of_range);
}

TEST(DailySetTest, RejectsOutOfRangeIndexForGet)
{
    const DailySet daily_set {make_desired_shifts(4)};

    EXPECT_THROW((void)daily_set.get(4), std::out_of_range);
}

TEST(DailySetTest, ReturnsRemainingCombinationCountForPartiallyFilledWeekendSet)
{
    const auto required_shifts = weekend_shifts::get_weekly_required_shifts();
    DailySet set {required_shifts};

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

TEST(DailySetTest, ReturnsIndexedCombinationForPartiallyFilledSet)
{
    std::vector<Shift> desired_shifts {weekend_shifts::get_off_shift(), weekend_shifts::get_off_shift(), weekend_shifts::get_weekend_early_shift_a()};

    DailySet set {desired_shifts};
    set.set(0, weekend_shifts::get_off_shift());

    const auto first = set.get_set(0);
    EXPECT_EQ(first.get(0).get_code(), "OFF");
    EXPECT_EQ(first.get(1).get_code(), "OFF");
    EXPECT_EQ(first.get(2).get_code(), "WE-EA");

    const auto second = set.get_set(1);
    EXPECT_EQ(second.get(0).get_code(), "OFF");
    EXPECT_EQ(second.get(1).get_code(), "WE-EA");
    EXPECT_EQ(second.get(2).get_code(), "OFF");

    EXPECT_THROW((void)set.get_set(2), std::out_of_range);
}

TEST(DailySetTest, ReturnsIndexedCombinationForPartiallyFilledSetWithSixDifferentShifts)
{
    const Shift shift_a {"A", StartTime {Hour {6}, Minute {0}}, StopTime {Hour {14}, Minute {0}}, 8.0};
    const Shift shift_b {"B", StartTime {Hour {7}, Minute {0}}, StopTime {Hour {15}, Minute {0}}, 8.0};
    const Shift shift_c {"C", StartTime {Hour {8}, Minute {0}}, StopTime {Hour {16}, Minute {0}}, 8.0};
    const Shift shift_d {"D", StartTime {Hour {9}, Minute {0}}, StopTime {Hour {17}, Minute {0}}, 8.0};
    const Shift shift_e {"E", StartTime {Hour {10}, Minute {0}}, StopTime {Hour {18}, Minute {0}}, 8.0};
    const Shift shift_f {"F", StartTime {Hour {11}, Minute {0}}, StopTime {Hour {19}, Minute {0}}, 8.0};

    DailySet set {{shift_a, shift_b, shift_c, shift_d, shift_e, shift_f}};
    set.set(1, shift_b);
    set.set(4, shift_e);

    EXPECT_EQ(set.get_nr_of_combinations(), 24u);

    const auto first = set.get_set(0);
    EXPECT_EQ(first.get(0).get_code(), "A");
    EXPECT_EQ(first.get(1).get_code(), "B");
    EXPECT_EQ(first.get(2).get_code(), "C");
    EXPECT_EQ(first.get(3).get_code(), "D");
    EXPECT_EQ(first.get(4).get_code(), "E");
    EXPECT_EQ(first.get(5).get_code(), "F");

    const auto middle = set.get_set(7);
    EXPECT_EQ(middle.get(0).get_code(), "C");
    EXPECT_EQ(middle.get(1).get_code(), "B");
    EXPECT_EQ(middle.get(2).get_code(), "A");
    EXPECT_EQ(middle.get(3).get_code(), "F");
    EXPECT_EQ(middle.get(4).get_code(), "E");
    EXPECT_EQ(middle.get(5).get_code(), "D");

    const auto last = set.get_set(23);
    EXPECT_EQ(last.get(0).get_code(), "F");
    EXPECT_EQ(last.get(1).get_code(), "B");
    EXPECT_EQ(last.get(2).get_code(), "D");
    EXPECT_EQ(last.get(3).get_code(), "C");
    EXPECT_EQ(last.get(4).get_code(), "E");
    EXPECT_EQ(last.get(5).get_code(), "A");

    EXPECT_THROW((void)set.get_set(24), std::out_of_range);
}

TEST(RosterValidationTest, RejectsLateShiftFollowedByEarlyShift)
{
    DailySet monday {{weekday_shifts::get_late_shift_2()}};
    monday.set(0, weekday_shifts::get_late_shift_2());

    DailySet tuesday {{weekday_shifts::get_early_shift_b()}};
    tuesday.set(0, weekday_shifts::get_early_shift_b());

    EXPECT_FALSE(verify_2_weekday_shifts_allowed_consecutively(monday, tuesday));
}

TEST(RosterValidationTest, AcceptsShiftPairsWithAtLeastTwelveHoursRest)
{
    DailySet monday {{weekday_shifts::get_day_shift()}};
    monday.set(0, weekday_shifts::get_day_shift());

    DailySet tuesday {{weekday_shifts::get_late_shift_1()}};
    tuesday.set(0, weekday_shifts::get_late_shift_1());

    EXPECT_TRUE(verify_2_weekday_shifts_allowed_consecutively(monday, tuesday));
}

TEST(RosterValidationTest, AcceptsOffDayBetweenWorkedDays)
{
    DailySet saturday {{weekend_shifts::get_weekend_late_shift()}};
    saturday.set(0, weekend_shifts::get_weekend_late_shift());

    DailySet sunday {{weekend_shifts::get_off_shift()}};
    sunday.set(0, weekend_shifts::get_off_shift());

    EXPECT_TRUE(verify_2_weekend_shifts_allowed_consecutively(saturday, sunday));
}

TEST(RosterValidationTest, AcceptsIdenticalWeekendShiftConsecutively)
{
    DailySet saturday {{weekend_shifts::get_weekend_split_late_shift()}};
    saturday.set(0, weekend_shifts::get_weekend_split_late_shift());

    DailySet sunday {{weekend_shifts::get_weekend_split_late_shift()}};
    sunday.set(0, weekend_shifts::get_weekend_split_late_shift());

    EXPECT_TRUE(verify_2_weekend_shifts_allowed_consecutively(saturday, sunday));
}

TEST(RosterValidationTest, RejectsTwoConsecutiveWeekdayOffDays)
{
    DailySet monday {{weekday_shifts::get_off_shift()}};
    monday.set(0, weekday_shifts::get_off_shift());

    DailySet tuesday {{weekday_shifts::get_off_shift()}};
    tuesday.set(0, weekday_shifts::get_off_shift());

    EXPECT_FALSE(verify_2_weekday_shifts_allowed_consecutively(monday, tuesday));
}

TEST(RosterValidationTest, AcceptsTwoConsecutiveWeekendOffDays)
{
    DailySet saturday {{weekend_shifts::get_off_shift()}};
    saturday.set(0, weekend_shifts::get_off_shift());

    DailySet sunday {{weekend_shifts::get_off_shift()}};
    sunday.set(0, weekend_shifts::get_off_shift());

    EXPECT_TRUE(verify_2_weekend_shifts_allowed_consecutively(saturday, sunday));
}

TEST(RosterValidationTest, AcceptsTwoConsecutiveOffDaysAcrossGeneralTransition)
{
    DailySet sunday {{weekend_shifts::get_off_shift()}};
    sunday.set(0, weekend_shifts::get_off_shift());

    DailySet monday {{weekday_shifts::get_off_shift()}};
    monday.set(0, weekday_shifts::get_off_shift());

    EXPECT_TRUE(verify_2_shifts_allowed_consecutively(sunday, monday));
}

} // namespace roster_slayer
