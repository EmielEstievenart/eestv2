#include "find_thursday.hpp"

#include "double_day_planning_validator.hpp"
#include "find_friday.hpp"
#include "weekday_shifts.hpp"

#include <cstddef>

void find_possible_thursdays(WeekPlanning planning, DaysOfTheWeek search_until, const SearchResultCallback& on_found)
{
    DoubleDayPlanningValidator validator;
    OneDayPlanning<WeekdayShiftCode> thursday_planning(get_weekday_required_shifts());

    auto nr_of_combinations = thursday_planning.get_nr_of_combinations();
    for (auto index = 0; index < nr_of_combinations; index++)
    {
        auto thursday_candidate = thursday_planning.get_set(index);
        bool is_valid           = true;

        for (std::size_t person = 0; person < thursday_candidate.size(); ++person)
        {
            const auto previous_shift = planning.getPreviousWeekdayShift(static_cast<int>(person), DaysOfTheWeek::thursday);
            if (previous_shift.has_value() && !validator.is_valid(*previous_shift, thursday_candidate.get(person).get_code()))
            {
                is_valid = false;
                break;
            }

            const auto next_shift = planning.getNextWeekdayShift(static_cast<int>(person), DaysOfTheWeek::thursday);
            if (next_shift.has_value() && !validator.is_valid(thursday_candidate.get(person).get_code(), *next_shift))
            {
                is_valid = false;
                break;
            }
        }

        if (!is_valid)
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
