#include "find_wednesday.hpp"

#include "double_day_planning_validator.hpp"
#include "find_thursday.hpp"
#include "weekday_shifts.hpp"

void find_possible_wednesdays(WeekPlanning planning, DaysOfTheWeek search_until, const SearchResultCallback& on_found)
{
    DoubleDayPlanningValidator validator;
    OneDayPlanning<WeekdayShiftCode> wednesday_planning(get_weekday_required_shifts());
    const bool validate_against_tuesday = planning.tuesday.has_value();

    auto nr_of_combinations = wednesday_planning.get_nr_of_combinations();
    for (auto index = 0; index < nr_of_combinations; index++)
    {
        auto wednesday_candidate = wednesday_planning.get_set(index);

        if (validate_against_tuesday && !validator.is_valid(*planning.tuesday, wednesday_candidate))
        {
            continue;
        }

        auto candidate_planning = planning;
        candidate_planning.wednesday.emplace(wednesday_candidate);

        if (search_until == DaysOfTheWeek::wednesday)
        {
            on_found(candidate_planning);
            continue;
        }

        find_possible_thursdays(candidate_planning, search_until, on_found);
    }
}
