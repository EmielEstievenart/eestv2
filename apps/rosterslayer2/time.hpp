#pragma once

#include <stdexcept>

struct Hour
{
    explicit Hour(int value) : value {value}
    {
        if (value < 0 || value > 23)
        {
            throw std::invalid_argument("Invalid hour value in shift configuration");
        }
    }

    int value;
};

struct Minute
{
    explicit Minute(int value) : value {value}
    {
        if (value < 0 || value > 59)
        {
            throw std::invalid_argument("Invalid minute value in shift configuration");
        }
    }

    int value;
};

struct HourMinuteTime
{

    HourMinuteTime(Hour h, Minute m) : hour(h.value), minute(m.value) { }

    int hour {0};
    int minute {0};
};