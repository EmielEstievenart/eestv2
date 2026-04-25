#pragma once

#include "shifts_enums.hpp"
#include "time.hpp"

#include <cstdint>

using StartTime = HourMinuteTime;
using StopTime  = HourMinuteTime;

enum class ShiftType
{
    is_early,
    is_day,
    is_late,
    is_off
};

template <typename shiftEnum>
struct Shift
{
public:
    Shift() = default;

    Shift(shiftEnum code, StartTime start_time, StopTime stop_time, double number_of_hours_worked, ShiftType type)
        : _code {code}, _start_time {start_time}, _stop_time {stop_time}, _number_of_hours_worked {number_of_hours_worked}, _type(type)
    {
    }

    std::uint16_t shift_id() const { return static_cast<std::uint16_t>(_code); }
    shiftEnum get_code() const { return _code; }

    shiftEnum _code;
    StartTime _start_time;
    StopTime _stop_time;
    double _number_of_hours_worked {0.0};
    ShiftType _type;
};
