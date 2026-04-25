#pragma once

#include "weekday_shifts.hpp"
#include "weekend_shifts.hpp"
#include "one_day_planning.hpp"

class DoubleDayPlanningValidator
{
public:
    bool is_valid(WeekendShiftCode first, WeekendShiftCode second);
    bool is_valid(WeekendShiftCode first, WeekdayShiftCode second);
    bool is_valid(WeekdayShiftCode first, WeekdayShiftCode second);
    bool is_valid(WeekdayShiftCode first, WeekendShiftCode second);
};
