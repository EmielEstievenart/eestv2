#include "find_monday.hpp"

#include "double_day_planning_validator.hpp"
#include "find_tuesday.hpp"
#include "weekday_shifts.hpp"

#include <iostream>

void find_possible_mondays(WeekPlanning planning, DaysOfTheWeek search_until)
{
    if (!planning.sunday.has_value())
    {
        return;
    }

    DoubleDayPlanningValidator validator;
    OneDayPlanning<WeekdayShiftCode> monday_planning(get_weekday_required_shifts());

    auto nr_of_combinations = monday_planning.get_nr_of_combinations();
    for (auto index = 0; index < nr_of_combinations; index++)
    {
        auto monday_candidate = monday_planning.get_set(index);

        if (!validator.is_valid(*planning.sunday, monday_candidate))
        {
            continue;
        }

        auto candidate_planning = planning;
        candidate_planning.monday.emplace(monday_candidate);

        if (search_until == DaysOfTheWeek::monday)
        {
            candidate_planning.print(std::cout);
            continue;
        }

        find_possible_tuesdays(candidate_planning, search_until);
    }
}
