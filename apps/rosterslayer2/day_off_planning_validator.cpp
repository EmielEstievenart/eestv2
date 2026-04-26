#include "day_off_planning_validator.hpp"

#include <cstddef>
#include <optional>

namespace
{

template <typename ShiftCode>
std::optional<ShiftType> get_shift_type(const std::optional<OneDayPlanning<ShiftCode>>& planning, std::size_t person)
{
    if (!planning.has_value() || person >= planning->size() || !planning->is_set(person))
    {
        return std::nullopt;
    }

    return planning->get(person)._type;
}

std::optional<ShiftType> get_previous_shift_type(const WeekPlanning& week_planning, std::size_t& person, DaysOfTheWeek& day)
{
    switch (day)
    {
    case DaysOfTheWeek::monday:
        if (!week_planning.sunday.has_value() || person >= week_planning.sunday->size())
        {
            return std::nullopt;
        }

        person = person == 0 ? week_planning.sunday->size() - 1 : person - 1;
        day    = DaysOfTheWeek::sunday;
        return get_shift_type(week_planning.sunday, person);

    case DaysOfTheWeek::tuesday:
        day = DaysOfTheWeek::monday;
        return get_shift_type(week_planning.monday, person);

    case DaysOfTheWeek::wednesday:
        day = DaysOfTheWeek::tuesday;
        return get_shift_type(week_planning.tuesday, person);

    case DaysOfTheWeek::thursday:
        day = DaysOfTheWeek::wednesday;
        return get_shift_type(week_planning.wednesday, person);

    case DaysOfTheWeek::friday:
        day = DaysOfTheWeek::thursday;
        return get_shift_type(week_planning.thursday, person);

    case DaysOfTheWeek::saturday:
        day = DaysOfTheWeek::friday;
        return get_shift_type(week_planning.friday, person);

    case DaysOfTheWeek::sunday:
        day = DaysOfTheWeek::saturday;
        return get_shift_type(week_planning.saturday, person);
    }

    return std::nullopt;
}

}

bool DayOffPlanningValidator::needs_day_off(const WeekPlanning& week_planning, int person, DaysOfTheWeek current_day, int max_consecutive_days)
{
    if (person < 0 || max_consecutive_days <= 0)
    {
        return false;
    }

    auto person_index = static_cast<std::size_t>(person);
    auto day          = current_day;

    for (int day_count = 0; day_count < max_consecutive_days; ++day_count)
    {
        const auto previous_shift_type = get_previous_shift_type(week_planning, person_index, day);
        if (!previous_shift_type.has_value() || *previous_shift_type == ShiftType::is_off)
        {
            return false;
        }
    }

    return true;
}
