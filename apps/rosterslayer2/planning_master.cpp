#include "planning_master.hpp"

#include "find_friday.hpp"
#include "find_monday.hpp"
#include "find_saturday.hpp"
#include "find_sunday.hpp"
#include "find_thursday.hpp"
#include "find_tuesday.hpp"
#include "find_wednesday.hpp"

void PlanningMaster::start_search(DaysOfTheWeek start_day, WeekPlanning planning, DaysOfTheWeek stop_day, SearchResultCallback on_found, SearchContext* context)
{
    switch (start_day)
    {
    case DaysOfTheWeek::monday:
        find_possible_mondays(planning, stop_day, on_found, context);
        break;
    case DaysOfTheWeek::tuesday:
        find_possible_tuesdays(planning, stop_day, on_found, context);
        break;
    case DaysOfTheWeek::wednesday:
        find_possible_wednesdays(planning, stop_day, on_found, context);
        break;
    case DaysOfTheWeek::thursday:
        find_possible_thursdays(planning, stop_day, on_found, context);
        break;
    case DaysOfTheWeek::friday:
        find_possible_fridays(planning, stop_day, on_found, context);
        break;
    case DaysOfTheWeek::saturday:
        find_possible_saturdays(planning, stop_day, on_found, context);
        break;
    case DaysOfTheWeek::sunday:
        find_possible_sundays(planning, stop_day, on_found, context);
        break;
    }
}
