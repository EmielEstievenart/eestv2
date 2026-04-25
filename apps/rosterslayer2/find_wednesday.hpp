#pragma once

#include "days_of_the_week.hpp"
#include "search_context.hpp"
#include "search_result_callback.hpp"

void find_possible_wednesdays(WeekPlanning planning, DaysOfTheWeek search_until, const SearchResultCallback& on_found, SearchContext* context = nullptr);
