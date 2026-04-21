#pragma once

#include "shift.hpp"

#include <vector>

namespace roster_slayer
{
namespace weekday_shifts
{

[[nodiscard]] inline Shift get_off_shift()
{
    return Shift("OFF", StartTime {Hour {0}, Minute {0}}, StopTime {Hour {0}, Minute {0}}, 0.0);
}

[[nodiscard]] inline Shift get_early_shift_a()
{
    return Shift("EA", StartTime {Hour {7}, Minute {0}}, StopTime {Hour {15}, Minute {15}}, 7.5);
}

[[nodiscard]] inline Shift get_early_shift_b()
{
    return Shift("EB", StartTime {Hour {6}, Minute {45}}, StopTime {Hour {15}, Minute {0}}, 7.5);
}

[[nodiscard]] inline Shift get_day_shift()
{
    return Shift("D", StartTime {Hour {10}, Minute {45}}, StopTime {Hour {19}, Minute {0}}, 7.5);
}

[[nodiscard]] inline Shift get_late_shift_1()
{
    return Shift("L1", StartTime {Hour {13}, Minute {0}}, StopTime {Hour {21}, Minute {0}}, 7.5);
}

[[nodiscard]] inline Shift get_late_shift_2()
{
    return Shift("L2", StartTime {Hour {13}, Minute {30}}, StopTime {Hour {21}, Minute {30}}, 7.5);
}

[[nodiscard]] inline const std::vector<Shift>& get_weekly_required_shifts()
{
    static const std::vector<Shift> shifts {get_off_shift(), get_off_shift(), get_off_shift(), get_early_shift_a(), get_early_shift_a(), get_early_shift_a(), get_early_shift_b(), get_day_shift(), get_late_shift_1(), get_late_shift_2()};

    return shifts;
}

} // namespace weekday_shifts
} // namespace roster_slayer
