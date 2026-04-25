#include "find_friday.hpp"

#include "double_day_planning_validator.hpp"
#include "find_saturday.hpp"
#include "weekday_shifts.hpp"

void find_possible_fridays(WeekPlanning planning, DaysOfTheWeek search_until, const SearchResultCallback& on_found)
{
    DoubleDayPlanningValidator validator;
    OneDayPlanning<WeekdayShiftCode> friday_planning(get_weekday_required_shifts());
    const bool validate_against_thursday = planning.thursday.has_value();

    auto nr_of_combinations = friday_planning.get_nr_of_combinations();
    for (auto index = 0; index < nr_of_combinations; index++)
    {
        auto friday_candidate = friday_planning.get_set(index);

        if (validate_against_thursday && !validator.is_valid(*planning.thursday, friday_candidate))
        {
            continue;
        }

        auto candidate_planning = planning;
        candidate_planning.friday.emplace(friday_candidate);

        if (search_until == DaysOfTheWeek::friday)
        {
            on_found(candidate_planning);
            continue;
        }

        find_possible_saturdays(candidate_planning, search_until, on_found);
    }
}
