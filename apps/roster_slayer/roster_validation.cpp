#include "roster_validation.hpp"

#include <cstddef>

namespace roster_slayer
{
namespace
{
constexpr int minutes_per_day      = 24 * 60;
constexpr int minimum_rest_minutes = 12 * 60;

[[nodiscard]] int to_minutes(const HourMinuteTime& time)
{
    return (time.hour * 60) + time.minute;
}

[[nodiscard]] bool shifts_have_required_rest(const Shift& first_shift, const Shift& second_shift)
{
    if (first_shift.is_off() || second_shift.is_off())
    {
        return true;
    }

    if (first_shift.get_code() == second_shift.get_code())
    {
        return true;
    }

    const int first_shift_end    = to_minutes(first_shift.end_time());
    const int second_shift_start = to_minutes(second_shift.start_time());
    const int rest_minutes       = (minutes_per_day - first_shift_end) + second_shift_start;

    return rest_minutes >= minimum_rest_minutes;
}

[[nodiscard]] bool verify_2_shifts_allowed_consecutively_impl(const DailySet& day_1, const DailySet& day_2, bool allow_consecutive_off_days)
{
    if (day_1.size() != day_2.size())
    {
        return false;
    }

    for (std::size_t person_index = 0; person_index < day_1.size(); ++person_index)
    {
        const Shift& first_shift  = day_1.get(person_index);
        const Shift& second_shift = day_2.get(person_index);

        if (!allow_consecutive_off_days && first_shift.is_off() && second_shift.is_off())
        {
            return false;
        }

        if (!shifts_have_required_rest(first_shift, second_shift))
        {
            return false;
        }
    }

    return true;
}
} // namespace

bool verify_2_shifts_allowed_consecutively(const DailySet& day_1, const DailySet& day_2)
{
    return verify_2_shifts_allowed_consecutively_impl(day_1, day_2, true);
}

bool verify_2_weekend_shifts_allowed_consecutively(const DailySet& weekend_day_1, const DailySet& weekend_day_2)
{
    return verify_2_shifts_allowed_consecutively_impl(weekend_day_1, weekend_day_2, true);
}

bool verify_2_weekday_shifts_allowed_consecutively(const DailySet& day_1, const DailySet& day_2)
{
    return verify_2_shifts_allowed_consecutively_impl(day_1, day_2, false);
}

} // namespace roster_slayer
