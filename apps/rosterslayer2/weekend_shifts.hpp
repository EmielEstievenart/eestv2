#pragma once

#include "shift.hpp"
#include "shifts_enums.hpp"
#include "time.hpp"

#include <vector>

using WeekendShift = Shift<WeekendShiftCode>;

inline WeekendShift get_off_shift()
{
    return WeekendShift(WeekendShiftCode::off, StartTime {Hour {0}, Minute {0}}, StopTime {Hour {0}, Minute {0}}, 0.0, ShiftType::is_off);
}

inline WeekendShift get_weekend_early_shift_a()
{
    return WeekendShift(WeekendShiftCode::early_a, StartTime {Hour {6}, Minute {45}}, StopTime {Hour {15}, Minute {15}}, 8.0, ShiftType::is_early);
}

inline WeekendShift get_weekend_early_shift_b()
{
    return WeekendShift(WeekendShiftCode::early_b, StartTime {Hour {7}, Minute {0}}, StopTime {Hour {15}, Minute {30}}, 8.0, ShiftType::is_early);
}

inline WeekendShift get_weekend_split_day_shift()
{
    return WeekendShift(WeekendShiftCode::split_day, StartTime {Hour {7}, Minute {45}}, StopTime {Hour {19}, Minute {0}}, 8.0, ShiftType::is_day);
}

inline WeekendShift get_weekend_split_late_shift()
{
    return WeekendShift(WeekendShiftCode::split_late, StartTime {Hour {7}, Minute {45}}, StopTime {Hour {20}, Minute {45}}, 8.0, ShiftType::is_late);
}

inline WeekendShift get_weekend_late_shift()
{
    return WeekendShift(WeekendShiftCode::late, StartTime {Hour {13}, Minute {0}}, StopTime {Hour {21}, Minute {30}}, 8.0, ShiftType::is_late);
}

inline const std::vector<WeekendShift>& get_weekend_required_shifts()
{
    static const std::vector<WeekendShift> shifts {
        get_off_shift(), get_off_shift(), get_off_shift(), get_off_shift(), get_off_shift(), get_weekend_early_shift_a(), get_weekend_early_shift_b(), get_weekend_split_day_shift(), get_weekend_split_late_shift(), get_weekend_late_shift()};

    return shifts;
}

inline const std::vector<WeekendShift>& get_weekend_unqiue_shifts()
{
    static const std::vector<WeekendShift> shifts {get_off_shift(), get_weekend_early_shift_a(), get_weekend_early_shift_b(), get_weekend_split_day_shift(), get_weekend_split_late_shift(), get_weekend_late_shift()};

    return shifts;
}