#include "find_thursday.hpp"

#include "double_day_planning_validator.hpp"
#include "find_friday.hpp"
#include "weekday_shifts.hpp"

#include <iostream>

void find_possible_thursdays(WeekPlanning planning, DaysOfTheWeek search_until)
{
    if (!planning.wednesday.has_value())
    {
        return;
    }

    DoubleDayPlanningValidator validator;
    OneDayPlanning<WeekdayShiftCode> thursday_planning(get_weekday_required_shifts());

    auto nr_of_combinations = thursday_planning.get_nr_of_combinations();
    for (auto index = 0; index < nr_of_combinations; index++)
    {
        auto thursday_candidate = thursday_planning.get_set(index);

        if (!validator.is_valid(*planning.wednesday, thursday_candidate))
        {
            continue;
        }

        auto candidate_planning = planning;
        candidate_planning.thursday.emplace(thursday_candidate);

        if (search_until == DaysOfTheWeek::thursday)
        {
            candidate_planning.print(std::cout);
            continue;
        }

        find_possible_fridays(candidate_planning, search_until);
    }
}
