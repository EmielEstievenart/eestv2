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

}

bool DoubleDayPlanningValidator::is_valid(WeekendShiftCode first, WeekendShiftCode second)
{
    return get_weekend_weekend_lookup_table()[static_cast<std::size_t>(first)][static_cast<std::size_t>(second)];
}

bool DoubleDayPlanningValidator::is_valid(WeekendShiftCode first, WeekdayShiftCode second)
{
    return get_weekend_weekday_lookup_table()[static_cast<std::size_t>(first)][static_cast<std::size_t>(second)];
}

bool DoubleDayPlanningValidator::is_valid(WeekdayShiftCode first, WeekdayShiftCode second)
{
    return get_weekday_weekday_lookup_table()[static_cast<std::size_t>(first)][static_cast<std::size_t>(second)];
}

bool DoubleDayPlanningValidator::is_valid(WeekdayShiftCode first, WeekendShiftCode second)
{
    return get_weekday_weekend_lookup_table()[static_cast<std::size_t>(first)][static_cast<std::size_t>(second)];
}
