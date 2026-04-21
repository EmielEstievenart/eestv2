#pragma once

#include "shift.hpp"

#include <vector>

namespace roster_slayer
{
namespace weekend_shifts
{

[[nodiscard]] inline Shift get_off_shift()
{
    return Shift("OFF", StartTime{Hour{0}, Minute{0}}, StopTime{Hour{0}, Minute{0}}, 0.0);
}

[[nodiscard]] inline Shift get_weekend_early_shift_a()
{
    return Shift("WE-EA", StartTime{Hour{6}, Minute{45}}, StopTime{Hour{15}, Minute{15}}, 8.0, true, false, false);
}

[[nodiscard]] inline Shift get_weekend_early_shift_b()
{
    return Shift("WE-EB", StartTime{Hour{7}, Minute{0}}, StopTime{Hour{15}, Minute{30}}, 8.0, true, false, false);
}

[[nodiscard]] inline Shift get_weekend_split_day_shift()
{
    return Shift("WE-SD", StartTime{Hour{7}, Minute{45}}, StopTime{Hour{19}, Minute{0}}, 8.0, false, true, false);
}

[[nodiscard]] inline Shift get_weekend_split_late_shift()
{
    return Shift("WE-SL", StartTime{Hour{7}, Minute{45}}, StopTime{Hour{20}, Minute{45}}, 8.0, false, false, true);
}

[[nodiscard]] inline Shift get_weekend_late_shift()
{
    return Shift("WE-L", StartTime{Hour{13}, Minute{0}}, StopTime{Hour{21}, Minute{30}}, 8.0, false, false, true);
}

[[nodiscard]] inline const std::vector<Shift>& get_weekly_required_shifts()
{
    static const std::vector<Shift> shifts{
        get_off_shift(),
        get_off_shift(),
        get_off_shift(),
        get_off_shift(),
        get_off_shift(),
        get_weekend_early_shift_a(),
        get_weekend_early_shift_b(),
        get_weekend_split_day_shift(),
        get_weekend_split_late_shift(),
        get_weekend_late_shift()};

    return shifts;
}

} // namespace weekend_shifts
} // namespace roster_slayer
