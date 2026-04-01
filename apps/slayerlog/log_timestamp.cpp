#include "log_timestamp.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>

namespace slayerlog
{

namespace
{

bool is_digit(char character)
{
    return std::isdigit(static_cast<unsigned char>(character)) != 0;
}

bool parse_fixed_digits(const std::string& text, std::size_t position, int digit_count, int& value)
{
    if ((position + static_cast<std::size_t>(digit_count)) > text.size())
    {
        return false;
    }

    int parsed_value = 0;
    for (int index = 0; index < digit_count; ++index)
    {
        const char character = text[position + static_cast<std::size_t>(index)];
        if (!is_digit(character))
        {
            return false;
        }

        parsed_value = (parsed_value * 10) + (character - '0');
    }

    value = parsed_value;
    return true;
}

bool is_leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

bool is_valid_date(int year, int month, int day)
{
    if (month < 1 || month > 12 || day < 1)
    {
        return false;
    }

    static constexpr int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int max_day = days_in_month[month - 1];
    if (month == 2 && is_leap_year(year))
    {
        max_day = 29;
    }

    return day <= max_day;
}

bool is_valid_time(int hour, int minute, int second)
{
    return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 && second >= 0 && second <= 59;
}

bool is_boundary_character(char character)
{
    return !std::isalnum(static_cast<unsigned char>(character));
}

struct ParsedTimestamp
{
    int year            = 0;
    int month           = 0;
    int day             = 0;
    int hour            = 0;
    int minute          = 0;
    int second          = 0;
    int nanoseconds     = 0;
    bool has_timezone   = false;
    int timezone_sign   = 0;
    int timezone_hour   = 0;
    int timezone_minute = 0;
};

struct ParseState
{
    std::size_t position = 0;
    bool bracketed       = false;
};

std::time_t make_utc_time(std::tm* utc_time)
{
#ifdef _WIN32
    return _mkgmtime(utc_time);
#else
    return timegm(utc_time);
#endif
}

std::optional<LogTimePoint> build_time_point(
    int year,
    int month,
    int day,
    int hour,
    int minute,
    int second,
    int nanoseconds,
    bool has_timezone,
    int timezone_sign,
    int timezone_hour,
    int timezone_minute)
{
    std::tm timestamp {};
    timestamp.tm_year  = year - 1900;
    timestamp.tm_mon   = month - 1;
    timestamp.tm_mday  = day;
    timestamp.tm_hour  = hour;
    timestamp.tm_min   = minute;
    timestamp.tm_sec   = second;
    timestamp.tm_isdst = -1;

    std::time_t epoch_seconds = has_timezone ? make_utc_time(&timestamp) : std::mktime(&timestamp);
    if (epoch_seconds == static_cast<std::time_t>(-1))
    {
        return std::nullopt;
    }

    if (has_timezone)
    {
        const int offset_seconds = ((timezone_hour * 60) + timezone_minute) * 60;
        epoch_seconds -= timezone_sign * offset_seconds;
    }

    const auto duration = std::chrono::seconds(epoch_seconds) + std::chrono::nanoseconds(nanoseconds);
    return LogTimePoint(std::chrono::duration_cast<LogTimePoint::duration>(duration));
}

void consume_leading_whitespace(const std::string& line, ParseState& state)
{
    while (state.position < line.size() && std::isspace(static_cast<unsigned char>(line[state.position])) != 0)
    {
        ++state.position;
    }
}

void parse_optional_brackets(const std::string& line, ParseState& state)
{
    state.bracketed = state.position < line.size() && line[state.position] == '[';
    if (state.bracketed)
    {
        ++state.position;
    }
}

bool consume_separator(const std::string& line, ParseState& state, char separator)
{
    if (state.position >= line.size() || line[state.position] != separator)
    {
        return false;
    }

    ++state.position;
    return true;
}

bool parse_date(const std::string& line, ParseState& state, ParsedTimestamp& parsed)
{
    if (!parse_fixed_digits(line, state.position, 4, parsed.year))
    {
        return false;
    }
    state.position += 4;

    if (!consume_separator(line, state, '-'))
    {
        return false;
    }

    if (!parse_fixed_digits(line, state.position, 2, parsed.month))
    {
        return false;
    }
    state.position += 2;

    if (!consume_separator(line, state, '-'))
    {
        return false;
    }

    if (!parse_fixed_digits(line, state.position, 2, parsed.day))
    {
        return false;
    }
    state.position += 2;

    return is_valid_date(parsed.year, parsed.month, parsed.day);
}

bool parse_time(const std::string& line, ParseState& state, ParsedTimestamp& parsed)
{
    if (state.position >= line.size() || (line[state.position] != 'T' && line[state.position] != ' '))
    {
        return false;
    }
    ++state.position;

    if (!parse_fixed_digits(line, state.position, 2, parsed.hour))
    {
        return false;
    }
    state.position += 2;

    if (!consume_separator(line, state, ':'))
    {
        return false;
    }

    if (!parse_fixed_digits(line, state.position, 2, parsed.minute))
    {
        return false;
    }
    state.position += 2;

    if (!consume_separator(line, state, ':'))
    {
        return false;
    }

    if (!parse_fixed_digits(line, state.position, 2, parsed.second))
    {
        return false;
    }
    state.position += 2;

    return is_valid_time(parsed.hour, parsed.minute, parsed.second);
}

bool parse_fractional_seconds(const std::string& line, ParseState& state, ParsedTimestamp& parsed)
{
    if (state.position >= line.size() || line[state.position] != '.')
    {
        return true;
    }

    ++state.position;

    const std::size_t fraction_start = state.position;
    while (state.position < line.size() && is_digit(line[state.position]))
    {
        ++state.position;
    }

    if (fraction_start == state.position)
    {
        return false;
    }

    const std::size_t fraction_length = std::min<std::size_t>(9, state.position - fraction_start);
    for (std::size_t index = 0; index < fraction_length; ++index)
    {
        parsed.nanoseconds = (parsed.nanoseconds * 10) + (line[fraction_start + index] - '0');
    }

    for (std::size_t index = fraction_length; index < 9; ++index)
    {
        parsed.nanoseconds *= 10;
    }

    return true;
}

bool parse_timezone(const std::string& line, ParseState& state, ParsedTimestamp& parsed)
{
    if (state.position >= line.size() || (line[state.position] != 'Z' && line[state.position] != '+' && line[state.position] != '-'))
    {
        return true;
    }

    parsed.has_timezone = true;
    if (line[state.position] == 'Z')
    {
        ++state.position;
        return true;
    }

    parsed.timezone_sign = line[state.position] == '+' ? 1 : -1;
    ++state.position;

    if (!parse_fixed_digits(line, state.position, 2, parsed.timezone_hour))
    {
        return false;
    }
    state.position += 2;

    if (state.position < line.size() && line[state.position] == ':')
    {
        ++state.position;
    }

    if (!parse_fixed_digits(line, state.position, 2, parsed.timezone_minute))
    {
        return false;
    }
    state.position += 2;

    return parsed.timezone_hour <= 23 && parsed.timezone_minute <= 59;
}

bool validate_trailing_boundary(const std::string& line, ParseState& state)
{
    if (state.bracketed)
    {
        if (!consume_separator(line, state, ']'))
        {
            return false;
        }
    }
    else if (state.position < line.size() && !is_boundary_character(line[state.position]))
    {
        return false;
    }

    return true;
}

} // namespace

std::optional<LogTimePoint> parse_log_timestamp(const std::string& line)
{
    ParseState state;
    ParsedTimestamp parsed;

    consume_leading_whitespace(line, state);
    parse_optional_brackets(line, state);

    if (!parse_date(line, state, parsed) || !parse_time(line, state, parsed) ||
        !parse_fractional_seconds(line, state, parsed) || !parse_timezone(line, state, parsed) ||
        !validate_trailing_boundary(line, state))
    {
        return std::nullopt;
    }

    return build_time_point(
        parsed.year,
        parsed.month,
        parsed.day,
        parsed.hour,
        parsed.minute,
        parsed.second,
        parsed.nanoseconds,
        parsed.has_timezone,
        parsed.timezone_sign,
        parsed.timezone_hour,
        parsed.timezone_minute);
}

} // namespace slayerlog
