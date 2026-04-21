#pragma once

#include "daily_set.hpp"

#include <string>
#include <vector>

namespace roster_slayer
{

[[nodiscard]] bool verify_2_shifts_allowed_consecutively(const DailySet& day_1, const DailySet& day_2);

[[nodiscard]] bool verify_2_weekend_shifts_allowed_consecutively(const DailySet& weekend_day_1, const DailySet& weekend_day_2);

[[nodiscard]] bool verify_2_weekday_shifts_allowed_consecutively(const DailySet& day_1, const DailySet& day_2);

[[nodiscard]] std::string explain_2_weekday_shift_rejection(const DailySet& day_1, const DailySet& day_2);

[[nodiscard]] bool verify_candidate_day_with_partial_week(const std::vector<DailySet>& valid_week, const DailySet& candidate_day);

[[nodiscard]] std::string explain_candidate_day_rejection(const std::vector<DailySet>& valid_week, const DailySet& candidate_day);

[[nodiscard]] bool verify_week_rollover(const std::vector<DailySet>& valid_week);

} // namespace roster_slayer
