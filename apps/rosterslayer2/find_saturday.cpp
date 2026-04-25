#include "find_saturday.hpp"

#include "find_sunday.hpp"
#include "week_planning.hpp"
#include "one_day_planning.hpp"
#include "weekend_shifts.hpp"

#include <iostream>

void find_possible_saturdays(DaysOfTheWeek search_until)
{
    WeekPlanning planning;
    OneDayPlanning<WeekendShiftCode> saturday_planning(get_weekend_required_shifts());

    saturday_planning.set(0, get_weekend_off_shift());
    saturday_planning.set(2, get_weekend_off_shift());
    saturday_planning.set(4, get_weekend_off_shift());
    saturday_planning.set(6, get_weekend_off_shift());
    saturday_planning.set(8, get_weekend_off_shift());

    auto nr_of_combinations = saturday_planning.get_nr_of_combinations();
    for (auto index = 0; index < nr_of_combinations; index++)
    {
        auto candidate_planning = planning;
        candidate_planning.saturday.emplace(saturday_planning.get_set(index));

        if (search_until == DaysOfTheWeek::saturday)
        {
            candidate_planning.print(std::cout);
            continue;
        }

        find_possible_sundays(candidate_planning, search_until);
    }
}
