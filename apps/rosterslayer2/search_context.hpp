#pragma once

#include "days_of_the_week.hpp"
#include "week_planning.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>

struct SearchProgress
{
    DaysOfTheWeek current_day {DaysOfTheWeek::monday};
    DaysOfTheWeek stop_day {DaysOfTheWeek::monday};
    std::uint64_t combination_index {0};
    std::uint64_t combination_count {0};
    std::size_t depth_from_start {0};
    DaysOfTheWeek deepest_valid_day {DaysOfTheWeek::monday};
    std::size_t found_count {0};
    bool cancelled {false};
    bool has_partial_planning {false};
    WeekPlanning partial_planning;
};

using SearchProgressCallback = std::function<void(const SearchProgress&)>;
using SearchCancelCallback   = std::function<bool()>;

struct SearchContext
{
    explicit SearchContext(DaysOfTheWeek start) : start_day(start), deepest_valid_day(start) { }

    DaysOfTheWeek start_day {DaysOfTheWeek::monday};
    SearchProgressCallback on_progress;
    SearchCancelCallback should_cancel;
    std::size_t found_count {0};
    std::size_t deepest_valid_depth {0};
    DaysOfTheWeek deepest_valid_day {DaysOfTheWeek::monday};
};

inline std::size_t day_index(DaysOfTheWeek day)
{
    return static_cast<std::size_t>(day);
}

inline std::size_t search_depth_from_start(DaysOfTheWeek start_day, DaysOfTheWeek current_day)
{
    const auto start   = day_index(start_day);
    const auto current = day_index(current_day);

    if (current >= start)
    {
        return current - start;
    }

    return current + 7 - start;
}

inline bool search_cancel_requested(const SearchContext* context)
{
    return context != nullptr && context->should_cancel && context->should_cancel();
}

inline void report_search_progress(SearchContext* context, DaysOfTheWeek current_day, DaysOfTheWeek stop_day, std::uint64_t combination_index,
                                   std::uint64_t combination_count, bool cancelled = false, const WeekPlanning* partial_planning = nullptr)
{
    if (context == nullptr || !context->on_progress)
    {
        return;
    }

    SearchProgress progress {
        current_day,
        stop_day,
        combination_index,
        combination_count,
        search_depth_from_start(context->start_day, current_day),
        context->deepest_valid_day,
        context->found_count,
        cancelled,
    };

    if (partial_planning != nullptr)
    {
        progress.has_partial_planning = true;
        progress.partial_planning     = *partial_planning;
    }

    context->on_progress(progress);
}

inline void mark_search_day_valid(SearchContext* context, DaysOfTheWeek day)
{
    if (context == nullptr)
    {
        return;
    }

    const auto depth = search_depth_from_start(context->start_day, day);
    if (depth >= context->deepest_valid_depth)
    {
        context->deepest_valid_depth = depth;
        context->deepest_valid_day   = day;
    }
}
