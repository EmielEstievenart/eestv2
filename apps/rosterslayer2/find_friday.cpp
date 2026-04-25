#include "find_friday.hpp"

#include "double_day_planning_validator.hpp"
#include "find_saturday.hpp"
#include "weekday_shifts.hpp"

#include <cstddef>

void find_possible_fridays(WeekPlanning planning, DaysOfTheWeek search_until, const SearchResultCallback& on_found)
{
    DoubleDayPlanningValidator validator;
    auto friday_planning = planning.friday.value_or(OneDayPlanning<WeekdayShiftCode>(get_weekday_required_shifts()));

    auto nr_of_combinations = friday_planning.get_nr_of_combinations();
    for (auto index = 0; index < nr_of_combinations; index++)
    {
        auto friday_candidate = friday_planning.get_set(index);
        bool is_valid         = true;

        for (std::size_t person = 0; person < friday_candidate.size(); ++person)
        {
            const auto previous_shift = planning.getPreviousWeekdayShift(static_cast<int>(person), DaysOfTheWeek::friday);
            if (previous_shift.has_value() && !validator.is_valid(*previous_shift, friday_candidate.get(person).get_code()))
            {
                is_valid = false;
                break;
            }

            const auto next_shift = planning.getNextWeekendShift(static_cast<int>(person), DaysOfTheWeek::friday);
            if (next_shift.has_value() && !validator.is_valid(friday_candidate.get(person).get_code(), *next_shift))
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
        candidate_planning.friday.emplace(friday_candidate);

        if (search_until == DaysOfTheWeek::friday)
        {
            on_found(candidate_planning);
            continue;
        }

        find_possible_saturdays(candidate_planning, search_until, on_found);
    }
}
