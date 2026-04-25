#pragma once

#include <cstdint>

enum class WeekdayShiftCode : std::uint8_t
{
    off,
    early_a,
    early_b,
    day,
    late1,
    late2,
};

enum class WeekendShiftCode : std::uint8_t
{
    off,
    early_a,
    early_b,
    split_day,
    split_late,
    late,
};
