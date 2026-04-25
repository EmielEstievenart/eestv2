#include "find_saturday.hpp"

#include "double_day_planning_validator.hpp"
#include "find_sunday.hpp"
#include "one_day_planning.hpp"
#include "weekend_shifts.hpp"

#include <cstddef>

void find_possible_saturdays(WeekPlanning planning, DaysOfTheWeek search_until, const SearchResultCallback& on_found)
{
    DoubleDayPlanningValidator validator;
    auto saturday_planning = planning.saturday.value_or(OneDayPlanning<WeekendShiftCode>(get_weekend_required_shifts()));

    auto nr_of_combinations = saturday_planning.get_nr_of_combinations();
    for (auto index = 0; index < nr_of_combinations; index++)
    {
        auto saturday_candidate = saturday_planning.get_set(index);
        bool is_valid           = true;

        for (std::size_t person = 0; person < saturday_candidate.size(); ++person)
        {
            const auto previous_shift = planning.getPreviousWeekdayShift(static_cast<int>(person), DaysOfTheWeek::saturday);
            if (previous_shift.has_value() && !validator.is_valid(*previous_shift, saturday_candidate.get(person).get_code()))
            {
                is_valid = false;
                break;
            }

            const auto next_shift = planning.getNextWeekendShift(static_cast<int>(person), DaysOfTheWeek::saturday);
            if (next_shift.has_value() && !validator.is_valid(saturday_candidate.get(person).get_code(), *next_shift))
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
        candidate_planning.saturday.emplace(saturday_candidate);

        if (search_until == DaysOfTheWeek::saturday)
        {
            on_found(candidate_planning);
            continue;
        }

        find_possible_sundays(candidate_planning, search_until, on_found);
    }
}
