#include "find_saturday.hpp"

#include "double_day_planning_validator.hpp"
#include "find_sunday.hpp"
#include "one_day_planning.hpp"
#include "weekend_shifts.hpp"

void find_possible_saturdays(WeekPlanning planning, DaysOfTheWeek search_until, const SearchResultCallback& on_found)
{
    DoubleDayPlanningValidator validator;
    OneDayPlanning<WeekendShiftCode> saturday_planning(get_weekend_required_shifts());
    const bool validate_against_friday = planning.friday.has_value();

    saturday_planning.set(0, get_weekend_off_shift());
    saturday_planning.set(2, get_weekend_off_shift());
    saturday_planning.set(4, get_weekend_off_shift());
    saturday_planning.set(6, get_weekend_off_shift());
    saturday_planning.set(8, get_weekend_off_shift());

    auto nr_of_combinations = saturday_planning.get_nr_of_combinations();
    for (auto index = 0; index < nr_of_combinations; index++)
    {
        auto saturday_candidate = saturday_planning.get_set(index);

        if (validate_against_friday && !validator.is_valid(*planning.friday, saturday_candidate))
        {
            continue;
        }

        auto candidate_planning = planning;
        candidate_planning.saturday.emplace(saturday_candidate);

        if (search_until == DaysOfTheWeek::saturday)
        {
            on_found(candidate_planning);
            continue;
        }

        find_possible_sundays(candidate_planning, search_until, on_found);
    }
}
