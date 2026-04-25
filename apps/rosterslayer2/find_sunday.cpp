#include "find_sunday.hpp"

#include "double_day_planning_validator.hpp"
#include "find_monday.hpp"
#include "weekend_shifts.hpp"

void find_possible_sundays(WeekPlanning planning, DaysOfTheWeek search_until, const SearchResultCallback& on_found)
{
    DoubleDayPlanningValidator validator;
    OneDayPlanning<WeekendShiftCode> sunday_planning(get_weekend_required_shifts());
    const bool validate_against_saturday = planning.saturday.has_value();

    sunday_planning.set(0, get_weekend_off_shift());
    sunday_planning.set(2, get_weekend_off_shift());
    sunday_planning.set(4, get_weekend_off_shift());
    sunday_planning.set(6, get_weekend_off_shift());
    sunday_planning.set(8, get_weekend_off_shift());

    auto nr_of_combinations = sunday_planning.get_nr_of_combinations();
    for (auto index = 0; index < nr_of_combinations; index++)
    {
        auto sunday_candidate = sunday_planning.get_set(index);

        if (validate_against_saturday && !validator.is_valid(*planning.saturday, sunday_candidate))
        {
            continue;
        }

        auto candidate_planning = planning;
        candidate_planning.sunday.emplace(sunday_candidate);

        if (search_until == DaysOfTheWeek::sunday)
        {
            on_found(candidate_planning);
            continue;
        }

        find_possible_mondays(candidate_planning, search_until, on_found);
    }
}
