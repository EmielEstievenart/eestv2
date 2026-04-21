#include "roster_validation.hpp"

#include <array>
#include <cstddef>
#include <iomanip>
#include <iostream>

namespace roster_slayer
{
namespace
{
constexpr int minutes_per_day      = 24 * 60;
constexpr int minimum_rest_minutes = 11 * 60;
constexpr std::size_t monday_index = 2;
constexpr std::size_t friday_index = 6;
constexpr std::array<const char*, 7> day_names {"Sat", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri"};

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

[[nodiscard]] bool is_day_off_for_person(const DailySet& day, std::size_t person_index)
{
    return day.get(person_index).is_off();
}

void print_week_for_debug(const std::vector<DailySet>& valid_week)
{
    if (valid_week.empty())
    {
        std::cout << "<empty week>\n";
        return;
    }

    constexpr int column_width = 8;

    std::cout << std::left << std::setw(column_width) << "Row";
    for (std::size_t day_index = 0; day_index < valid_week.size(); ++day_index)
    {
        const char* day_name = day_index < day_names.size() ? day_names[day_index] : "Day";
        std::cout << std::setw(column_width) << day_name;
    }
    std::cout << "\n";

    const std::size_t row_count = valid_week.front().size();
    for (std::size_t row_index = 0; row_index < row_count; ++row_index)
    {
        std::cout << std::setw(column_width) << row_index;
        for (const DailySet& day : valid_week)
        {
            std::cout << std::setw(column_width) << day.get(row_index).get_code();
        }
        std::cout << "\n";
    }
}

bool fail_week_rollover_check(const std::vector<DailySet>& valid_week, const std::string& reason)
{
    std::cout << "Final rollover check failed: " << reason << "\n";
    print_week_for_debug(valid_week);
    std::cout << "\n";
    return false;
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

[[nodiscard]] std::string explain_2_shifts_allowed_consecutively_impl(const DailySet& day_1, const DailySet& day_2, bool allow_consecutive_off_days)
{
    if (day_1.size() != day_2.size())
    {
        return "day sizes differ";
    }

    for (std::size_t person_index = 0; person_index < day_1.size(); ++person_index)
    {
        const Shift& first_shift  = day_1.get(person_index);
        const Shift& second_shift = day_2.get(person_index);

        if (!allow_consecutive_off_days && first_shift.is_off() && second_shift.is_off())
        {
            return "person " + std::to_string(person_index) + " has consecutive weekday OFF days";
        }

        if (!shifts_have_required_rest(first_shift, second_shift))
        {
            return "person " + std::to_string(person_index) + " has insufficient rest between " + first_shift.get_code() + " and " + second_shift.get_code();
        }
    }

    return {};
}

[[nodiscard]] std::string explain_candidate_day_with_partial_week_impl(const std::vector<DailySet>& valid_week, const DailySet& candidate_day)
{
    if (valid_week.empty())
    {
        return {};
    }

    if (valid_week.back().size() != candidate_day.size())
    {
        return "candidate day size differs from existing week";
    }

    const std::size_t candidate_day_index = valid_week.size();

    for (std::size_t person_index = 0; person_index < candidate_day.size(); ++person_index)
    {
        std::size_t weekday_off_days = 0;
        if (candidate_day_index >= monday_index)
        {
            const std::size_t last_weekday_index = std::min(friday_index, candidate_day_index);

            for (std::size_t day_index = monday_index; day_index <= last_weekday_index; ++day_index)
            {
                const bool is_off = (day_index == candidate_day_index) ? candidate_day.get(person_index).is_off() : is_day_off_for_person(valid_week[day_index], person_index);
                if (is_off)
                {
                    ++weekday_off_days;
                }
            }

            if (weekday_off_days > 1)
            {
                return "person " + std::to_string(person_index) + " has more than 1 OFF day between Monday and Friday";
            }
        }

        if (candidate_day_index >= 2)
        {
            const bool previous_previous_day_is_off = is_day_off_for_person(valid_week[candidate_day_index - 2], person_index);
            const bool previous_day_is_off          = is_day_off_for_person(valid_week[candidate_day_index - 1], person_index);
            const bool candidate_day_is_off         = candidate_day.get(person_index).is_off();

            if (previous_previous_day_is_off && previous_day_is_off && candidate_day_is_off)
            {
                return "person " + std::to_string(person_index) + " has 3 consecutive OFF days";
            }
        }
    }

    return {};
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

std::string explain_2_weekday_shift_rejection(const DailySet& day_1, const DailySet& day_2)
{
    return explain_2_shifts_allowed_consecutively_impl(day_1, day_2, false);
}

bool verify_candidate_day_with_partial_week(const std::vector<DailySet>& valid_week, const DailySet& candidate_day)
{
    return explain_candidate_day_with_partial_week_impl(valid_week, candidate_day).empty();
}

std::string explain_candidate_day_rejection(const std::vector<DailySet>& valid_week, const DailySet& candidate_day)
{
    return explain_candidate_day_with_partial_week_impl(valid_week, candidate_day);
}

bool verify_week_rollover(const std::vector<DailySet>& valid_week)
{
    if (valid_week.size() != 7)
    {
        return fail_week_rollover_check(valid_week, "week does not contain 7 days");
    }

    const std::size_t row_count = valid_week.front().size();
    for (const DailySet& day : valid_week)
    {
        if (day.size() != row_count)
        {
            return fail_week_rollover_check(valid_week, "day sizes are inconsistent");
        }
    }

    for (std::size_t person_index = 0; person_index < row_count; ++person_index)
    {
        const std::size_t previous_person_index = (person_index + row_count - 1) % row_count;

        DailySet friday {{valid_week[6].get(person_index)}};
        friday.set(0, valid_week[6].get(person_index));

        DailySet next_saturday {{valid_week[0].get(previous_person_index)}};
        next_saturday.set(0, valid_week[0].get(previous_person_index));

        if (!verify_2_shifts_allowed_consecutively(friday, next_saturday))
        {
            return fail_week_rollover_check(valid_week, "person " + std::to_string(person_index) + " has invalid Friday-to-next-Saturday transition with previous person " + std::to_string(previous_person_index));
        }

        const Shift& tuesday      = valid_week[3].get(person_index);
        const Shift& wednesday    = valid_week[4].get(person_index);
        const Shift& thursday     = valid_week[5].get(person_index);
        const Shift& friday_shift = valid_week[6].get(person_index);
        const Shift& saturday     = valid_week[0].get(previous_person_index);
        const Shift& sunday       = valid_week[1].get(previous_person_index);
        const Shift& monday       = valid_week[2].get(previous_person_index);
        const Shift& next_tuesday = valid_week[3].get(previous_person_index);

        const std::array<const Shift*, 8> boundary_sequence {&tuesday, &wednesday, &thursday, &friday_shift, &saturday, &sunday, &monday, &next_tuesday};

        for (std::size_t start_index = 0; start_index + 2 < boundary_sequence.size(); ++start_index)
        {
            if (boundary_sequence[start_index]->is_off() && boundary_sequence[start_index + 1]->is_off() && boundary_sequence[start_index + 2]->is_off())
            {
                return fail_week_rollover_check(valid_week, "person " + std::to_string(person_index) + " has 3 consecutive OFF days across rollover starting at boundary index " + std::to_string(start_index));
            }
        }

        for (std::size_t start_index = 0; start_index + 4 < boundary_sequence.size(); ++start_index)
        {
            if (!boundary_sequence[start_index]->is_off() && !boundary_sequence[start_index + 1]->is_off() && !boundary_sequence[start_index + 2]->is_off() && !boundary_sequence[start_index + 3]->is_off() &&
                !boundary_sequence[start_index + 4]->is_off())
            {
                return fail_week_rollover_check(valid_week, "person " + std::to_string(person_index) + " has 5 consecutive work days across rollover starting at boundary index " + std::to_string(start_index));
            }
        }
    }

    return true;
}

} // namespace roster_slayer
