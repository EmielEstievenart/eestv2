#include "find_saturday.hpp"

#include "find_sunday.hpp"
#include "week_planning.hpp"
#include "one_day_planning.hpp"
#include "weekend_shifts.hpp"

void find_possible_saturdays()
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
        planning.saturday.emplace(saturday_planning.get_set(index));
        find_possible_sundays(planning);
    }
}
