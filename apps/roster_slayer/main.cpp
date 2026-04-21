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
[[nodiscard]] bool find_sunday(std::vector<roster_slayer::DailySet>& valid_week);

[[nodiscard]] bool find_monday(std::vector<roster_slayer::DailySet>& valid_week);

[[nodiscard]] bool find_tuesday(std::vector<roster_slayer::DailySet>& valid_week);

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

[[nodiscard]] bool try_to_add_day(std::vector<roster_slayer::DailySet>& valid_week, const roster_slayer::DailySet& day_template, DayValidator validator, NextDayFinder next_day_finder = nullptr)
{
    const std::uint64_t combination_count = day_template.get_nr_of_combinations();
    for (std::uint64_t combination_index = 0; combination_index < combination_count; ++combination_index)
    {
        const auto candidate_day = day_template.get_set(combination_index);
        if (!validator(valid_week.back(), candidate_day))
        {
            continue;
        }

        valid_week.push_back(candidate_day);
        if ((next_day_finder == nullptr) || next_day_finder(valid_week))
        {
            return true;
        }

        valid_week.pop_back();
    }

    return false;
}

[[nodiscard]] std::vector<roster_slayer::DailySet> find_saturday()
{
    auto saturday_template = make_weekend_template();

    saturday_template.set(0, roster_slayer::weekend_shifts::get_off_shift());
    saturday_template.set(2, roster_slayer::weekend_shifts::get_off_shift());
    saturday_template.set(4, roster_slayer::weekend_shifts::get_off_shift());
    saturday_template.set(6, roster_slayer::weekend_shifts::get_off_shift());
    saturday_template.set(8, roster_slayer::weekend_shifts::get_off_shift());

    const std::uint64_t combination_count = saturday_template.get_nr_of_combinations();
    for (std::uint64_t combination_index = 0; combination_index < combination_count; ++combination_index)
    {
        std::vector<roster_slayer::DailySet> valid_week;
        valid_week.push_back(saturday_template.get_set(combination_index));

        if (find_sunday(valid_week))
        {
            return valid_week;
        }
    }

    return {};
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
    return try_to_add_day(valid_week, make_weekday_template(), roster_slayer::verify_2_shifts_allowed_consecutively, find_tuesday);
}

[[nodiscard]] bool find_tuesday(std::vector<roster_slayer::DailySet>& valid_week)
{
    return try_to_add_day(valid_week, make_weekday_template(), roster_slayer::verify_2_weekday_shifts_allowed_consecutively);
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
