#pragma once

#include "weekday_shifts.hpp"
#include "weekend_shifts.hpp"
#include "one_day_planning.hpp"

class DoubleDayPlanningValidator
{
public:
    bool is_valid(const OneDayPlanning<WeekendShiftCode>& first, const OneDayPlanning<WeekendShiftCode>& second);
    bool is_valid(const OneDayPlanning<WeekendShiftCode>& first, const OneDayPlanning<WeekdayShiftCode>& second);
    bool is_valid(const OneDayPlanning<WeekdayShiftCode>& first, const OneDayPlanning<WeekdayShiftCode>& second);
    bool is_valid(const OneDayPlanning<WeekdayShiftCode>& first, const OneDayPlanning<WeekendShiftCode>& second);
};
