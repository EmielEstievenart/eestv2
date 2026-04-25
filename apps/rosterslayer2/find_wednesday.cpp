#include "find_wednesday.hpp"

#include "double_day_planning_validator.hpp"
#include "find_thursday.hpp"
#include "weekday_shifts.hpp"

#include <cstddef>

void find_possible_wednesdays(WeekPlanning planning, DaysOfTheWeek search_until, const SearchResultCallback& on_found)
{
    DoubleDayPlanningValidator validator;
    auto wednesday_planning = planning.wednesday.value_or(OneDayPlanning<WeekdayShiftCode>(get_weekday_required_shifts()));

    auto nr_of_combinations = wednesday_planning.get_nr_of_combinations();
    for (auto index = 0; index < nr_of_combinations; index++)
    {
        auto wednesday_candidate = wednesday_planning.get_set(index);
        bool is_valid            = true;

        for (std::size_t person = 0; person < wednesday_candidate.size(); ++person)
        {
            const auto previous_shift = planning.getPreviousWeekdayShift(static_cast<int>(person), DaysOfTheWeek::wednesday);
            if (previous_shift.has_value() && !validator.is_valid(*previous_shift, wednesday_candidate.get(person).get_code()))
            {
                is_valid = false;
                break;
            }

            const auto next_shift = planning.getNextWeekdayShift(static_cast<int>(person), DaysOfTheWeek::wednesday);
            if (next_shift.has_value() && !validator.is_valid(wednesday_candidate.get(person).get_code(), *next_shift))
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
        candidate_planning.wednesday.emplace(wednesday_candidate);

        if (search_until == DaysOfTheWeek::wednesday)
        {
            on_found(candidate_planning);
            continue;
        }

        find_possible_thursdays(candidate_planning, search_until, on_found);
    }
}
