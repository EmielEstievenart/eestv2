#include "find_monday.hpp"

#include "double_day_planning_validator.hpp"
#include "find_tuesday.hpp"
#include "weekday_shifts.hpp"

void find_possible_mondays(WeekPlanning planning, DaysOfTheWeek search_until, const SearchResultCallback& on_found)
{
    OneDayPlanning<WeekdayShiftCode> monday_planning(get_weekday_required_shifts());
    DoubleDayPlanningValidator validator;
    const bool validate_against_sunday = planning.sunday.has_value();

    auto nr_of_combinations = monday_planning.get_nr_of_combinations();
    for (auto index = 0; index < nr_of_combinations; index++)
    {
        auto monday_candidate = monday_planning.get_set(index);

        if (validate_against_sunday && !validator.is_valid(*planning.sunday, monday_candidate))
        {
            continue;
        }

        auto candidate_planning = planning;
        candidate_planning.monday.emplace(monday_candidate);

        if (search_until == DaysOfTheWeek::monday)
        {
            on_found(candidate_planning);
            continue;
        }

        find_possible_tuesdays(candidate_planning, search_until, on_found);
    }
}
