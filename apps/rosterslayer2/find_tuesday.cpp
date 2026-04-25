#include "find_tuesday.hpp"

#include "double_day_planning_validator.hpp"
#include "find_wednesday.hpp"
#include "weekday_shifts.hpp"

#include <iostream>

void find_possible_tuesdays(WeekPlanning planning, DaysOfTheWeek search_until)
{
    if (!planning.monday.has_value())
    {
        return;
    }

    DoubleDayPlanningValidator validator;
    OneDayPlanning<WeekdayShiftCode> tuesday_planning(get_weekday_required_shifts());

    auto nr_of_combinations = tuesday_planning.get_nr_of_combinations();
    for (auto index = 0; index < nr_of_combinations; index++)
    {
        auto tuesday_candidate = tuesday_planning.get_set(index);

        if (!validator.is_valid(*planning.monday, tuesday_candidate))
        {
            continue;
        }

        auto candidate_planning = planning;
        candidate_planning.tuesday.emplace(tuesday_candidate);

        if (search_until == DaysOfTheWeek::tuesday)
        {
            candidate_planning.print(std::cout);
            continue;
        }

        find_possible_wednesdays(candidate_planning, search_until);
    }
}
