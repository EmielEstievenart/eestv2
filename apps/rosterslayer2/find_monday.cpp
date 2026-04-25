#include "find_monday.hpp"

#include "double_day_planning_validator.hpp"
#include "find_tuesday.hpp"
#include "weekday_shifts.hpp"

#include <cstddef>

void find_possible_mondays(WeekPlanning planning, DaysOfTheWeek search_until, const SearchResultCallback& on_found, SearchContext* context)
{
    auto monday_planning = planning.monday.value_or(OneDayPlanning<WeekdayShiftCode>(get_weekday_required_shifts()));
    DoubleDayPlanningValidator validator;

    auto nr_of_combinations = monday_planning.get_nr_of_combinations();
    for (auto index = 0; index < nr_of_combinations; index++)
    {
        if (search_cancel_requested(context))
        {
            report_search_progress(context, DaysOfTheWeek::monday, search_until, index, nr_of_combinations, true);
            return;
        }

        report_search_progress(context, DaysOfTheWeek::monday, search_until, index, nr_of_combinations);

        auto monday_candidate = monday_planning.get_set(index);
        bool is_valid         = true;

        for (std::size_t person = 0; person < monday_candidate.size(); ++person)
        {
            const auto previous_shift = planning.getPreviousWeekendShift(static_cast<int>(person), DaysOfTheWeek::monday);
            if (previous_shift.has_value() && !validator.is_valid(*previous_shift, monday_candidate.get(person).get_code()))
            {
                is_valid = false;
                break;
            }

            const auto next_shift = planning.getNextWeekdayShift(static_cast<int>(person), DaysOfTheWeek::monday);
            if (next_shift.has_value() && !validator.is_valid(monday_candidate.get(person).get_code(), *next_shift))
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
        candidate_planning.monday.emplace(monday_candidate);
        mark_search_day_valid(context, DaysOfTheWeek::monday);
        report_search_progress(context, DaysOfTheWeek::monday, search_until, index, nr_of_combinations, false, &candidate_planning);

        if (search_until == DaysOfTheWeek::monday)
        {
            if (context != nullptr)
            {
                ++context->found_count;
                report_search_progress(context, DaysOfTheWeek::monday, search_until, index, nr_of_combinations, false, &candidate_planning);
            }
            on_found(candidate_planning);
            continue;
        }

        if (search_cancel_requested(context))
        {
            report_search_progress(context, DaysOfTheWeek::monday, search_until, index, nr_of_combinations, true);
            return;
        }

        find_possible_tuesdays(candidate_planning, search_until, on_found, context);
    }
}
