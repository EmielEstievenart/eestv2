#pragma once

#include "weekday_shifts.hpp"
#include "weekend_shifts.hpp"
#include "one_day_planning.hpp"

class DoubleDayPlanningValidator
{
    bool DoubleDayPlanningValidator::is_valid(OneDayPlanning<WeekendShift> weekend, OneDayPlanning<WeekendShift>);
    bool DoubleDayPlanningValidator::is_valid(OneDayPlanning<WeekendShift> weekend, OneDayPlanning<WeekdayShift>);
    bool DoubleDayPlanningValidator::is_valid(OneDayPlanning<WeekdayShift> weekend, OneDayPlanning<WeekdayShift>);
    bool DoubleDayPlanningValidator::is_valid(OneDayPlanning<WeekdayShift> weekend, OneDayPlanning<WeekendShift>);
};