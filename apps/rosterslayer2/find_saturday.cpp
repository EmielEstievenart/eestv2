#include "find_saturday.hpp"

#include "day_off_planning_validator.hpp"
#include "double_day_planning_validator.hpp"
#include "find_sunday.hpp"
#include "one_day_planning.hpp"
#include "weekend_shifts.hpp"

#include <cstddef>

void find_possible_saturdays(WeekPlanning planning, DaysOfTheWeek search_until, const SearchResultCallback& on_found, SearchContext* context)
{
    DoubleDayPlanningValidator validator;
    auto saturday_planning = planning.saturday.value_or(OneDayPlanning<WeekendShiftCode>(get_weekend_required_shifts()));
    if (!DayOffPlanningValidator::apply_mandatory_days_off(planning, saturday_planning, DaysOfTheWeek::saturday, DayOffPlanningValidator::default_max_consecutive_days, get_weekend_off_shift))
    {
        return;
    }

    auto nr_of_combinations = saturday_planning.get_nr_of_combinations();
    for (auto index = 0; index < nr_of_combinations; index++)
    {
        if (search_cancel_requested(context))
        {
            report_search_progress(context, DaysOfTheWeek::saturday, search_until, index, nr_of_combinations, true);
            return;
        }

        report_search_progress(context, DaysOfTheWeek::saturday, search_until, index, nr_of_combinations);

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
        mark_search_day_valid(context, DaysOfTheWeek::saturday);
        report_search_progress(context, DaysOfTheWeek::saturday, search_until, index, nr_of_combinations, false, &candidate_planning);

        if (search_until == DaysOfTheWeek::saturday)
        {
            if (context != nullptr)
            {
                ++context->found_count;
                report_search_progress(context, DaysOfTheWeek::saturday, search_until, index, nr_of_combinations, false, &candidate_planning);
            }
            on_found(candidate_planning);
            continue;
        }

        if (search_cancel_requested(context))
        {
            report_search_progress(context, DaysOfTheWeek::saturday, search_until, index, nr_of_combinations, true);
            return;
        }

        find_possible_sundays(candidate_planning, search_until, on_found, context);
    }
}
