#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <vector>

#include "daily_set.hpp"
#include "roster_validation.hpp"
#include "weekday_shifts.hpp"
#include "weekend_shifts.hpp"

namespace
{
std::uint64_t printed_week_count = 0;

[[nodiscard]] bool find_sunday(std::vector<roster_slayer::DailySet>& valid_week);

[[nodiscard]] bool find_monday(std::vector<roster_slayer::DailySet>& valid_week);

[[nodiscard]] bool find_tuesday(std::vector<roster_slayer::DailySet>& valid_week);

[[nodiscard]] bool find_wednesday(std::vector<roster_slayer::DailySet>& valid_week);

[[nodiscard]] bool find_thursday(std::vector<roster_slayer::DailySet>& valid_week);

[[nodiscard]] std::vector<roster_slayer::DailySet> find_saturday();

using DayValidator = bool (*)(const roster_slayer::DailySet& current_day, const roster_slayer::DailySet& candidate_day);

using NextDayFinder = bool (*)(std::vector<roster_slayer::DailySet>& valid_week);

constexpr std::array<const char*, 7> day_names {"Sat", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri"};

[[nodiscard]] roster_slayer::DailySet make_weekday_template()
{
    return roster_slayer::DailySet {roster_slayer::weekday_shifts::get_weekly_required_shifts()};
}

[[nodiscard]] roster_slayer::DailySet make_weekend_template()
{
    return roster_slayer::DailySet {roster_slayer::weekend_shifts::get_weekly_required_shifts()};
}

void force_off_after_four_consecutive_work_days(const std::vector<roster_slayer::DailySet>& valid_week, roster_slayer::DailySet& day_template, const roster_slayer::Shift& off_shift)
{
    if (valid_week.size() < 4)
    {
        return;
    }

    const std::size_t current_day_index = valid_week.size();
    if (current_day_index < 4)
    {
        return;
    }

    for (std::size_t person_index = 0; person_index < day_template.size(); ++person_index)
    {
        bool worked_previous_four_days = true;
        for (std::size_t day_offset = 4; day_offset > 0; --day_offset)
        {
            if (valid_week[current_day_index - day_offset].get(person_index).is_off())
            {
                worked_previous_four_days = false;
                break;
            }
        }

        if (worked_previous_four_days)
        {
            day_template.set(person_index, off_shift);
        }
    }
}

void print_valid_week(const std::vector<roster_slayer::DailySet>& valid_week)
{
    if (valid_week.empty())
    {
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

        for (const auto& day : valid_week)
        {
            std::cout << std::setw(column_width) << day.get(row_index).get_code();
        }

        std::cout << "\n";
    }
}

void print_thursday_rejection(const std::vector<roster_slayer::DailySet>& valid_week, const roster_slayer::DailySet& candidate_day, const std::string& reason)
{
    if (valid_week.size() != 5)
    {
        return;
    }

    std::cout << "Thursday candidate rejected: " << reason << "\n";

    auto rejected_week = valid_week;
    rejected_week.push_back(candidate_day);
    print_valid_week(rejected_week);
    std::cout << "\n";
}

[[nodiscard]] bool try_to_add_day(std::vector<roster_slayer::DailySet>& valid_week, const roster_slayer::DailySet& day_template, DayValidator validator, NextDayFinder next_day_finder = nullptr)
{
    bool found_valid_week                 = false;
    const std::uint64_t combination_count = day_template.get_nr_of_combinations();
    for (std::uint64_t combination_index = 0; combination_index < combination_count; ++combination_index)
    {
        const auto candidate_day = day_template.get_set(combination_index);
        if (!validator(valid_week.back(), candidate_day))
        {
            // print_thursday_rejection(valid_week, candidate_day, roster_slayer::explain_2_weekday_shift_rejection(valid_week.back(), candidate_day));
            continue;
        }

        if (!roster_slayer::verify_candidate_day_with_partial_week(valid_week, candidate_day))
        {
            // print_thursday_rejection(valid_week, candidate_day, roster_slayer::explain_candidate_day_rejection(valid_week, candidate_day));
            continue;
        }

        valid_week.push_back(candidate_day);
        if (next_day_finder == nullptr)
        {
            ++printed_week_count;
            std::cout << "Valid week " << printed_week_count << ":\n";
            print_valid_week(valid_week);
            std::cout << "\n";
            found_valid_week = true;
        }
        else if (next_day_finder(valid_week))
        {
            found_valid_week = true;
        }

        valid_week.pop_back();
    }

    return found_valid_week;
}

[[nodiscard]] std::vector<roster_slayer::DailySet> find_saturday()
{
    auto saturday_template = make_weekend_template();

    saturday_template.set(0, roster_slayer::weekend_shifts::get_off_shift());
    saturday_template.set(2, roster_slayer::weekend_shifts::get_off_shift());
    saturday_template.set(4, roster_slayer::weekend_shifts::get_off_shift());
    saturday_template.set(6, roster_slayer::weekend_shifts::get_off_shift());
    saturday_template.set(8, roster_slayer::weekend_shifts::get_off_shift());

    std::vector<roster_slayer::DailySet> first_valid_week;
    const std::uint64_t combination_count = saturday_template.get_nr_of_combinations();
    for (std::uint64_t combination_index = 0; combination_index < combination_count; ++combination_index)
    {
        std::vector<roster_slayer::DailySet> valid_week;
        valid_week.push_back(saturday_template.get_set(combination_index));

        if (find_sunday(valid_week) && first_valid_week.empty())
        {
            first_valid_week = valid_week;
        }
    }

    return first_valid_week;
}

[[nodiscard]] bool find_sunday(std::vector<roster_slayer::DailySet>& valid_week)
{
    auto sunday_template = make_weekend_template();

    sunday_template.set(0, roster_slayer::weekend_shifts::get_off_shift());
    sunday_template.set(2, roster_slayer::weekend_shifts::get_off_shift());
    sunday_template.set(4, roster_slayer::weekend_shifts::get_off_shift());
    sunday_template.set(6, roster_slayer::weekend_shifts::get_off_shift());
    sunday_template.set(8, roster_slayer::weekend_shifts::get_off_shift());

    return try_to_add_day(valid_week, sunday_template, roster_slayer::verify_2_weekend_shifts_allowed_consecutively, find_monday);
}

[[nodiscard]] bool find_monday(std::vector<roster_slayer::DailySet>& valid_week)
{
    std::vector<roster_slayer::DailySet> possible_sets;
    auto monday_template = make_weekday_template();
    //Check for previous shifts and start a couple of async threads that continue.
    for (int person = 0; person < 10; person++)
    {
        if (valid_week.at(1).get(person).is_off())
        {
        }
        else if (valid_week.at(1).get(person).is_late())
        {
            monday_template.set(person, roster_slayer::weekday_shifts::get_late_shift_1());
            //And now branch off with this one and continue till other persons are set. Then try_to_add_day
            monday_template.set(person, roster_slayer::weekday_shifts::get_late_shift_2());
            //And now branch off with this one and continue till other persons are set. Then try_to_add_day
            monday_template.set(person, roster_slayer::weekday_shifts::get_day_shift());
            //And now branch off with this one and continue till other persons are set. Then try_to_add_day
        }
    }

    return try_to_add_day(valid_week, make_weekday_template(), roster_slayer::verify_2_shifts_allowed_consecutively, find_tuesday);
}

[[nodiscard]] bool find_tuesday(std::vector<roster_slayer::DailySet>& valid_week)
{
    return try_to_add_day(valid_week, make_weekday_template(), roster_slayer::verify_2_weekday_shifts_allowed_consecutively, find_wednesday);
}

[[nodiscard]] bool find_wednesday(std::vector<roster_slayer::DailySet>& valid_week)
{
    auto wednesday_template = make_weekday_template();
    force_off_after_four_consecutive_work_days(valid_week, wednesday_template, roster_slayer::weekday_shifts::get_off_shift());

    return try_to_add_day(valid_week, wednesday_template, roster_slayer::verify_2_weekday_shifts_allowed_consecutively, find_thursday);
}

[[nodiscard]] bool find_thursday(std::vector<roster_slayer::DailySet>& valid_week)
{
    auto thursday_template = make_weekday_template();
    force_off_after_four_consecutive_work_days(valid_week, thursday_template, roster_slayer::weekday_shifts::get_off_shift());

    return try_to_add_day(valid_week, thursday_template, roster_slayer::verify_2_weekday_shifts_allowed_consecutively);
}
} // namespace

int main()
{
    const auto valid_week = find_saturday();
    std::cout << "Days found in first valid week: " << valid_week.size() << "\n";

    if (!valid_week.empty())
    {
        print_valid_week(valid_week);
    }
}
