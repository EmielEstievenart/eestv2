#include "find_friday.hpp"

#include "double_day_planning_validator.hpp"
#include "weekday_shifts.hpp"

#include <iostream>

void find_possible_fridays(WeekPlanning planning, DaysOfTheWeek search_until)
{
    if (!planning.thursday.has_value())
    {
        return;
    }

    DoubleDayPlanningValidator validator;
    OneDayPlanning<WeekdayShiftCode> friday_planning(get_weekday_required_shifts());

    auto nr_of_combinations = friday_planning.get_nr_of_combinations();
    for (auto index = 0; index < nr_of_combinations; index++)
    {
        auto friday_candidate = friday_planning.get_set(index);

        if (!validator.is_valid(*planning.thursday, friday_candidate))
        {
            continue;
        }

        planning.friday.emplace(friday_candidate);

        if (search_until == DaysOfTheWeek::friday)
        {
            planning.print(std::cout);
            continue;
        }
    }
}
