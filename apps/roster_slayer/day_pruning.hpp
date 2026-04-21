#pragma once

#include "daily_set.hpp"
#include "weekday_shifts.hpp"

#include <cstddef>
#include <vector>

namespace roster_slayer
{
namespace day_pruning
{

[[nodiscard]] inline std::vector<Shift> get_allowed_next_weekday_shifts(const Shift& previous_shift)
{
    if (previous_shift.is_late())
    {
        return {weekday_shifts::get_off_shift(), weekday_shifts::get_day_shift(), weekday_shifts::get_late_shift_1(), weekday_shifts::get_late_shift_2()};
    }

    if (previous_shift.is_early())
    {
        return {weekday_shifts::get_off_shift(), weekday_shifts::get_early_shift_a(), weekday_shifts::get_early_shift_b(), weekday_shifts::get_day_shift()};
    }

    return {weekday_shifts::get_off_shift(),
            weekday_shifts::get_early_shift_a(),
            weekday_shifts::get_early_shift_b(),
            weekday_shifts::get_day_shift(),
            weekday_shifts::get_late_shift_1(),
            weekday_shifts::get_late_shift_2()};
}

[[nodiscard]] inline bool is_allowed_next_weekday_shift(const Shift& previous_shift, const Shift& candidate_shift)
{
    for (const Shift& allowed_shift : get_allowed_next_weekday_shifts(previous_shift))
    {
        if (allowed_shift.get_code() == candidate_shift.get_code())
        {
            return true;
        }
    }

    return false;
}

inline void append_possible_weekday_templates_recursive(const DailySet& previous_day,
                                                        std::size_t person_index,
                                                        DailySet current_template,
                                                        std::vector<DailySet>& possible_sets)
{
    if (person_index == current_template.size())
    {
        possible_sets.push_back(current_template);
        return;
    }

    const Shift& previous_shift = previous_day.get(person_index);
    if (current_template.is_set(person_index))
    {
        if (is_allowed_next_weekday_shift(previous_shift, current_template.get(person_index)))
        {
            append_possible_weekday_templates_recursive(previous_day, person_index + 1, current_template, possible_sets);
        }

        return;
    }

    for (const Shift& allowed_shift : get_allowed_next_weekday_shifts(previous_shift))
    {
        DailySet next_template = current_template;
        next_template.set(person_index, allowed_shift);
        append_possible_weekday_templates_recursive(previous_day, person_index + 1, next_template, possible_sets);
    }
}

[[nodiscard]] inline std::vector<DailySet> make_possible_weekday_templates(const DailySet& previous_day, const DailySet& day_template)
{
    std::vector<DailySet> possible_sets;
    append_possible_weekday_templates_recursive(previous_day, 0, day_template, possible_sets);
    return possible_sets;
}

} // namespace day_pruning
} // namespace roster_slayer
