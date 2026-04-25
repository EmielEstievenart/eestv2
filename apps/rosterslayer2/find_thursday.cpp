#include "find_thursday.hpp"

#include "double_day_planning_validator.hpp"
#include "find_friday.hpp"
#include "weekday_shifts.hpp"

void find_possible_thursdays(WeekPlanning planning, DaysOfTheWeek search_until, const SearchResultCallback& on_found)
{
    DoubleDayPlanningValidator validator;
    OneDayPlanning<WeekdayShiftCode> thursday_planning(get_weekday_required_shifts());
    const bool validate_against_wednesday = planning.wednesday.has_value();

    auto nr_of_combinations = thursday_planning.get_nr_of_combinations();
    for (auto index = 0; index < nr_of_combinations; index++)
    {
        auto thursday_candidate = thursday_planning.get_set(index);

        if (validate_against_wednesday && !validator.is_valid(*planning.wednesday, thursday_candidate))
        {
            continue;
        }

        auto candidate_planning = planning;
        candidate_planning.thursday.emplace(thursday_candidate);

        if (search_until == DaysOfTheWeek::thursday)
        {
            on_found(candidate_planning);
            continue;
        }

        find_possible_fridays(candidate_planning, search_until, on_found);
    }
}
