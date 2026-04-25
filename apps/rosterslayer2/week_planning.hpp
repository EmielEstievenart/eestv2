#pragma once

#include "one_day_planning.hpp"
#include "weekday_shifts.hpp"
#include "weekend_shifts.hpp"

#include <optional>

struct WeekPlanning
{
    std::optional<OneDayPlanning<WeekendShiftCode>> saturday;
    std::optional<OneDayPlanning<WeekendShiftCode>> sunday;
    std::optional<OneDayPlanning<WeekdayShiftCode>> monday;
    std::optional<OneDayPlanning<WeekdayShiftCode>> tuesday;
    std::optional<OneDayPlanning<WeekdayShiftCode>> wednesday;
    std::optional<OneDayPlanning<WeekdayShiftCode>> thursday;
    std::optional<OneDayPlanning<WeekdayShiftCode>> friday;
};
