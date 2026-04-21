#pragma once

#include "daily_set.hpp"

namespace roster_slayer
{

[[nodiscard]] bool verify_2_shifts_allowed_consecutively(const DailySet& day_1, const DailySet& day_2);

[[nodiscard]] bool verify_2_weekend_shifts_allowed_consecutively(const DailySet& weekend_day_1, const DailySet& weekend_day_2);

[[nodiscard]] bool verify_2_weekday_shifts_allowed_consecutively(const DailySet& day_1, const DailySet& day_2);

} // namespace roster_slayer
