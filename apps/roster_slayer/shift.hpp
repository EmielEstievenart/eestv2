#pragma once

#include <stdexcept>
#include <string>
#include <iomanip>
#include <sstream>
#include <utility>

namespace roster_slayer
{
struct Hour
{
    explicit Hour(int value)
        : value{value}
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
    explicit Minute(int value)
        : value{value}
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
    int hour {0};
    int minute {0};

    void assert_is_valid() const
    {
        if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
        {
            throw std::invalid_argument("Invalid time value in shift configuration");
        }
    }
};

[[nodiscard]] inline std::string to_string(const HourMinuteTime& value);

struct StartTime : HourMinuteTime
{
    StartTime() = default;

    StartTime(Hour hour, Minute minute)
    {
        this->hour = hour.value;
        this->minute = minute.value;
    }

    StartTime(int hour, int minute)
    {
        this->hour = Hour{hour}.value;
        this->minute = Minute{minute}.value;
    }
};

struct StopTime : HourMinuteTime
{
    StopTime() = default;

    StopTime(Hour hour, Minute minute)
    {
        this->hour = hour.value;
        this->minute = minute.value;
    }

    StopTime(int hour, int minute)
    {
        this->hour = Hour{hour}.value;
        this->minute = Minute{minute}.value;
    }
};

class Shift
{
public:
    Shift() = default;

    Shift(std::string code, StartTime start_time, StopTime stop_time, double number_of_hours_worked)
        : _start_time{start_time}
        , _end_time{stop_time}
        , _code{std::move(code)}
        , _number_of_hours_worked{number_of_hours_worked}
    {
        if (_number_of_hours_worked < 0.0)
        {
            throw std::invalid_argument("Shift must contain a positive number of hours");
        }

        if (_number_of_hours_worked == 0.0 && _code != "OFF")
        {
            throw std::invalid_argument("Only OFF shifts can have zero worked hours");
        }
    }

    const StartTime& start_time() const
    {
        return _start_time;
    }

    const StopTime& end_time() const
    {
        return _end_time;
    }

    [[nodiscard]] const std::string& get_code() const
    {
        return _code;
    }

    [[nodiscard]] double number_of_hours_worked() const
    {
        return _number_of_hours_worked;
    }

    [[nodiscard]] bool is_off() const
    {
        return _code == "OFF";
    }

    [[nodiscard]] std::string start_time_to_string() const
    {
        return to_string(_start_time);
    }

    [[nodiscard]] std::string end_time_to_string() const
    {
        return to_string(_end_time);
    }

private:
    StartTime _start_time;
    StopTime _end_time;
    std::string _code;
    double _number_of_hours_worked {0.0};
};

[[nodiscard]] inline std::string to_string(const HourMinuteTime& value)
{
    std::ostringstream out;
    out << std::setw(2) << std::setfill('0') << value.hour << ':' << std::setw(2) << value.minute;
    return out.str();
}

[[nodiscard]] inline std::string to_string(const Shift& value)
{
    return to_string(value.start_time()) + " -> " + to_string(value.end_time());
}
} // namespace roster_slayer
