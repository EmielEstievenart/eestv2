#include "find_tuesday.hpp"

#include "double_day_planning_validator.hpp"
#include "find_wednesday.hpp"
#include "weekday_shifts.hpp"

void find_possible_tuesdays(WeekPlanning planning, DaysOfTheWeek search_until, const SearchResultCallback& on_found)
{
    DoubleDayPlanningValidator validator;
    OneDayPlanning<WeekdayShiftCode> tuesday_planning(get_weekday_required_shifts());
    const bool validate_against_monday = planning.monday.has_value();

    auto nr_of_combinations = tuesday_planning.get_nr_of_combinations();
    for (auto index = 0; index < nr_of_combinations; index++)
    {
        auto tuesday_candidate = tuesday_planning.get_set(index);

        if (validate_against_monday && !validator.is_valid(*planning.monday, tuesday_candidate))
        {
            continue;
        }

        auto candidate_planning = planning;
        candidate_planning.tuesday.emplace(tuesday_candidate);

        if (search_until == DaysOfTheWeek::tuesday)
        {
            on_found(candidate_planning);
            continue;
        }

        find_possible_wednesdays(candidate_planning, search_until, on_found);
    }
}
