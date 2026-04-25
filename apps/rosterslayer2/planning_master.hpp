#pragma once

#include "days_of_the_week.hpp"
#include "search_result_callback.hpp"

class PlanningMaster
{
public:
    PlanningMaster() = default;

    void start_search(DaysOfTheWeek start_day, WeekPlanning planning, DaysOfTheWeek stop_day, SearchResultCallback on_found);
};
