#pragma once

#include "days_of_the_week.hpp"
#include "week_planning.hpp"

#include <cstddef>
#include <exception>

class DayOffPlanningValidator
{
public:
    static constexpr int default_max_consecutive_days = 4;

    static bool needs_day_off(const WeekPlanning& week_planning, int person, DaysOfTheWeek day_to_check, int max_consecutive_days);

    template <typename ShiftCode, typename OffShiftFactory>
    static bool apply_mandatory_days_off(const WeekPlanning& week_planning, OneDayPlanning<ShiftCode>& day_planning, DaysOfTheWeek day_to_check, int max_consecutive_days, OffShiftFactory get_off_shift)
    {
        for (std::size_t person = 0; person < day_planning.size(); ++person)
        {
            if (!needs_day_off(week_planning, static_cast<int>(person), day_to_check, max_consecutive_days))
            {
                continue;
            }

            try
            {
                day_planning.set(person, get_off_shift());
            }
            catch (const std::exception&)
            {
                return false;
            }
        }

        return true;
    }
};
