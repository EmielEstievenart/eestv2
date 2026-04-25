#include "double_day_planning_validator.hpp"

#include "one_day_planning.hpp"
#include "shift.hpp"
#include "weekday_shifts.hpp"
#include "weekend_shifts.hpp"

#include <cstddef>
#include <vector>

namespace
{

bool is_week_type_allowed_consecutively(ShiftType one, ShiftType two)
{
    if (one == ShiftType::is_off && two == ShiftType::is_off)
    {
        return false;
    }

    if (one == ShiftType::is_early && two == ShiftType::is_late)
    {
        return false;
    }

    if (one == ShiftType::is_late && two == ShiftType::is_early)
    {
        return false;
    }

    return true;
}

bool is_weekend_type_allowed_consecutively(ShiftType one, ShiftType two)
{
    if (one == ShiftType::is_early && two == ShiftType::is_late)
    {
        return false;
    }

    if (one == ShiftType::is_late && two == ShiftType::is_early)
    {
        return false;
    }

    return true;
}

template <typename FirstShift, typename SecondShift, typename Rule>
std::vector<std::vector<bool>> build_lookup_table(const std::vector<FirstShift>& first_shifts, const std::vector<SecondShift>& second_shifts, Rule rule)
{
    std::vector<std::vector<bool>> lookup_table;
    lookup_table.reserve(first_shifts.size());

    for (const auto& first_shift : first_shifts)
    {
        std::vector<bool> row;
        row.reserve(second_shifts.size());

        for (const auto& second_shift : second_shifts)
        {
            row.push_back(rule(first_shift._type, second_shift._type));
        }

        lookup_table.push_back(row);
    }

    return lookup_table;
}

const std::vector<std::vector<bool>>& get_weekend_weekend_lookup_table()
{
    static const auto table = build_lookup_table(get_weekend_unqiue_shifts(), get_weekend_unqiue_shifts(), is_weekend_type_allowed_consecutively);

    return table;
}

const std::vector<std::vector<bool>>& get_weekend_weekday_lookup_table()
{
    static const auto table = build_lookup_table(get_weekend_unqiue_shifts(), get_weekday_unique_shifts(), is_week_type_allowed_consecutively);

    return table;
}

const std::vector<std::vector<bool>>& get_weekday_weekday_lookup_table()
{
    static const auto table = build_lookup_table(get_weekday_unique_shifts(), get_weekday_unique_shifts(), is_week_type_allowed_consecutively);

    return table;
}

const std::vector<std::vector<bool>>& get_weekday_weekend_lookup_table()
{
    static const auto table = build_lookup_table(get_weekday_unique_shifts(), get_weekend_unqiue_shifts(), is_week_type_allowed_consecutively);

    return table;
}

template <typename FirstPlanning, typename SecondPlanning>
bool is_valid_using_lookup_table(const FirstPlanning& first, const SecondPlanning& second, const std::vector<std::vector<bool>>& table)
{
    if (first.size() != second.size())
    {
        return false;
    }

    for (std::size_t person = 0; person < first.size(); ++person)
    {
        const auto first_shift_index  = first.get(person);
        const auto second_shift_index = second.get(person);

        if (!table[first_shift_index][second_shift_index])
        {
            return false;
        }
    }

    return true;
}

}

bool DoubleDayPlanningValidator::is_valid(OneDayPlanning<WeekendShift> weekend_one, OneDayPlanning<WeekendShift> weekend_two)
{
    return is_valid_using_lookup_table(weekend_one, weekend_two, get_weekend_weekend_lookup_table());
}

bool DoubleDayPlanningValidator::is_valid(OneDayPlanning<WeekendShift> weekend, OneDayPlanning<WeekdayShift> weekday)
{
    return is_valid_using_lookup_table(weekend, weekday, get_weekend_weekday_lookup_table());
}

bool DoubleDayPlanningValidator::is_valid(OneDayPlanning<WeekdayShift> weekday_one, OneDayPlanning<WeekdayShift> weekday_two)
{
    return is_valid_using_lookup_table(weekday_one, weekday_two, get_weekday_weekday_lookup_table());
}

bool DoubleDayPlanningValidator::is_valid(OneDayPlanning<WeekdayShift> weekday, OneDayPlanning<WeekendShift> weekend)
{
    return is_valid_using_lookup_table(weekday, weekend, get_weekday_weekend_lookup_table());
}