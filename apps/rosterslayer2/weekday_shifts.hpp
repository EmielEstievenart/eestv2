#pragma once

#include "shift.hpp"

#include <vector>

using WeekdayShift = Shift<WeekdayShiftCode>;

inline WeekdayShift get_off_shift()
{
    return WeekdayShift(WeekdayShiftCode::off, StartTime {Hour {0}, Minute {0}}, StopTime {Hour {0}, Minute {0}}, 0.0, ShiftType::is_off);
}

inline WeekdayShift get_early_shift_a()
{
    return WeekdayShift(WeekdayShiftCode::early_a, StartTime {Hour {7}, Minute {0}}, StopTime {Hour {15}, Minute {15}}, 7.5, ShiftType::is_early);
}

inline WeekdayShift get_early_shift_b()
{
    return WeekdayShift(WeekdayShiftCode::early_b, StartTime {Hour {6}, Minute {45}}, StopTime {Hour {15}, Minute {0}}, 7.5, ShiftType::is_early);
}

inline WeekdayShift get_day_shift()
{
    return WeekdayShift(WeekdayShiftCode::day, StartTime {Hour {10}, Minute {45}}, StopTime {Hour {19}, Minute {0}}, 7.5, ShiftType::is_day);
}

inline WeekdayShift get_late_shift_1()
{
    return WeekdayShift(WeekdayShiftCode::late1, StartTime {Hour {13}, Minute {0}}, StopTime {Hour {21}, Minute {0}}, 7.5, ShiftType::is_late);
}

inline WeekdayShift get_late_shift_2()
{
    return WeekdayShift(WeekdayShiftCode::late2, StartTime {Hour {13}, Minute {30}}, StopTime {Hour {21}, Minute {30}}, 7.5, ShiftType::is_late);
}

inline const std::vector<WeekdayShift>& get_weekday_required_shifts()
{
    static const std::vector<WeekdayShift> shifts {get_off_shift(),     get_off_shift(),     get_off_shift(), get_early_shift_a(), get_early_shift_a(),
                                                   get_early_shift_a(), get_early_shift_b(), get_day_shift(), get_late_shift_1(),  get_late_shift_2()};

    return shifts;
}

inline const std::vector<WeekdayShift>& get_weekday_unique_shifts()
{
    static const std::vector<WeekdayShift> shifts {get_off_shift(), get_early_shift_a(), get_early_shift_b(), get_day_shift(), get_late_shift_1(), get_late_shift_2()};

    return shifts;
}
