#include <gtest/gtest.h>

#include "eestv/timestamp/timestamp_parser.hpp"

namespace eestv
{

namespace
{

bool apply_parser(const compiledDataAndTimeParser& parser, const std::string& input, DateAndTime& output, int start_index = 0, int* end_index = nullptr)
{
    std::string to_parse = input;
    int index            = start_index;

    for (const auto& step : parser.dateParser)
    {
        int index_jump = 0;

        if (!step(to_parse, index, index_jump, output))
        {
            return false;
        }

        index += index_jump;
    }

    if (end_index != nullptr)
    {
        *end_index = index;
    }

    return true;
}

bool find_parser_match(const compiledDataAndTimeParser& parser, const std::string& input, DateAndTime& output, int* match_start = nullptr, int* match_end = nullptr)
{
    for (int start_index = 0; start_index < static_cast<int>(input.size()); ++start_index)
    {
        DateAndTime candidate;
        int end_index = 0;

        if (!apply_parser(parser, input, candidate, start_index, &end_index))
        {
            continue;
        }

        output = candidate;
        if (match_start != nullptr)
        {
            *match_start = start_index;
        }

        if (match_end != nullptr)
        {
            *match_end = end_index;
        }

        return true;
    }

    return false;
}

} // namespace

TEST(TimestampParserTest, ParsesTwoDigitYearToken)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YY");

    ASSERT_EQ(compiled.dateParser.size(), 1U);

    std::string input = "26";
    DateAndTime output;
    int index_jump = 0;

    EXPECT_TRUE(compiled.dateParser.front()(input, 0, index_jump, output));
    EXPECT_EQ(output.year, 2026);
    EXPECT_EQ(index_jump, 2);
}

TEST(TimestampParserTest, ParsesFourDigitYearToken)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY");

    ASSERT_EQ(compiled.dateParser.size(), 1U);

    std::string input = "2026";
    DateAndTime output;
    int index_jump = 0;

    EXPECT_TRUE(compiled.dateParser.front()(input, 0, index_jump, output));
    EXPECT_EQ(output.year, 2026);
    EXPECT_EQ(index_jump, 4);
}

TEST(TimestampParserTest, RejectsMissingOpeningBracket)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("[YYYY-MM-DD hh:mm:ss]");

    DateAndTime output;

    EXPECT_FALSE(apply_parser(compiled, "2026-04-13 17:42:58]", output));
}

TEST(TimestampParserTest, ParsesBracketedDateAndTime)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("[YYYY-MM-DD hh:mm:ss]");

    DateAndTime output;

    EXPECT_TRUE(apply_parser(compiled, "[2026-04-13 17:42:58]", output));
    EXPECT_EQ(output.year, 2026);
    EXPECT_EQ(output.month, 4U);
    EXPECT_EQ(output.day, 13U);
    EXPECT_EQ(output.hour, 17U);
    EXPECT_EQ(output.minute, 42U);
    EXPECT_EQ(output.second, 58U);
    EXPECT_FALSE(output.leap_second);
}

TEST(TimestampParserTest, ParsesLeapSecond)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY-MM-DDThh:mm:ss");

    DateAndTime output;

    EXPECT_TRUE(apply_parser(compiled, "2026-12-31T23:59:60", output));
    EXPECT_EQ(output.second, 60U);
    EXPECT_TRUE(output.leap_second);
}

TEST(TimestampParserTest, ParsesTenthsOfASecond)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY-MM-DDThh:mm:ss.f");

    DateAndTime output;

    EXPECT_TRUE(apply_parser(compiled, "2026-12-31T23:59:58.1", output));
    EXPECT_EQ(output.nanosecond, 100000000U);
}

TEST(TimestampParserTest, ParsesMilliseconds)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY-MM-DDThh:mm:ss.fff");

    DateAndTime output;

    EXPECT_TRUE(apply_parser(compiled, "2026-12-31T23:59:58.123", output));
    EXPECT_EQ(output.nanosecond, 123000000U);
}

TEST(TimestampParserTest, ParsesNanoseconds)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY-MM-DDThh:mm:ss.fffffffff");

    DateAndTime output;

    EXPECT_TRUE(apply_parser(compiled, "2026-12-31T23:59:58.123456789", output));
    EXPECT_EQ(output.nanosecond, 123456789U);
}

TEST(TimestampParserTest, RejectsFractionWhenDigitIsMissing)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY-MM-DDThh:mm:ss.fff");

    DateAndTime output;

    EXPECT_FALSE(apply_parser(compiled, "2026-12-31T23:59:58.12", output));
}

TEST(TimestampParserTest, RejectsOutOfRangeMonth)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY-MM-DD hh:mm:ss");

    DateAndTime output;

    EXPECT_FALSE(apply_parser(compiled, "2026-13-13 17:42:58", output));
}

// -----------------------------------------------------------------------------
// New tests for common log-file formats and time zone handling
// Assumed new tokens:
//   MMM  -> Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec
//   Z    -> UTC designator 'Z'
//   ZZ   -> UTC offset ±HHMM
//   ZZZ  -> UTC offset ±HH:MM
// Assumed DateAndTime field:
//   std::optional<int> utc_offset_minutes
// -----------------------------------------------------------------------------

TEST(TimestampParserTest, ParsesUtcZuluDesignator)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY-MM-DDThh:mm:ssZ");

    DateAndTime output;

    EXPECT_TRUE(apply_parser(compiled, "2026-04-13T17:42:58Z", output));
    EXPECT_EQ(output.year, 2026);
    EXPECT_EQ(output.month, 4U);
    EXPECT_EQ(output.day, 13U);
    EXPECT_EQ(output.hour, 17U);
    EXPECT_EQ(output.minute, 42U);
    EXPECT_EQ(output.second, 58U);
    ASSERT_TRUE(output.utc_offset_minutes.has_value());
    EXPECT_EQ(*output.utc_offset_minutes, 0);
}

TEST(TimestampParserTest, ParsesTimezoneOffsetWithoutColon)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY-MM-DDThh:mm:ssZZ");

    DateAndTime output;

    EXPECT_TRUE(apply_parser(compiled, "2026-04-13T17:42:58+0230", output));
    ASSERT_TRUE(output.utc_offset_minutes.has_value());
    EXPECT_EQ(*output.utc_offset_minutes, 150);
}

TEST(TimestampParserTest, ParsesNegativeTimezoneOffsetWithoutColon)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY-MM-DDThh:mm:ssZZ");

    DateAndTime output;

    EXPECT_TRUE(apply_parser(compiled, "2026-04-13T17:42:58-0530", output));
    ASSERT_TRUE(output.utc_offset_minutes.has_value());
    EXPECT_EQ(*output.utc_offset_minutes, -330);
}

TEST(TimestampParserTest, ParsesTimezoneOffsetWithColon)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY-MM-DDThh:mm:ssZZZ");

    DateAndTime output;

    EXPECT_TRUE(apply_parser(compiled, "2026-04-13T17:42:58+02:30", output));
    ASSERT_TRUE(output.utc_offset_minutes.has_value());
    EXPECT_EQ(*output.utc_offset_minutes, 150);
}

TEST(TimestampParserTest, ParsesFractionalSecondsWithTimezoneOffset)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY-MM-DDThh:mm:ss.ffffffZZZ");

    DateAndTime output;

    EXPECT_TRUE(apply_parser(compiled, "2026-04-13T17:42:58.123456+02:00", output));
    EXPECT_EQ(output.nanosecond, 123456000U);
    ASSERT_TRUE(output.utc_offset_minutes.has_value());
    EXPECT_EQ(*output.utc_offset_minutes, 120);
}

TEST(TimestampParserTest, RejectsTimezoneOffsetHourOutOfRange)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY-MM-DDThh:mm:ssZZ");

    DateAndTime output;

    EXPECT_FALSE(apply_parser(compiled, "2026-04-13T17:42:58+2500", output));
}

TEST(TimestampParserTest, RejectsTimezoneOffsetMinuteOutOfRange)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY-MM-DDThh:mm:ssZZ");

    DateAndTime output;

    EXPECT_FALSE(apply_parser(compiled, "2026-04-13T17:42:58+1260", output));
}

TEST(TimestampParserTest, RejectsIncompleteTimezoneOffset)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY-MM-DDThh:mm:ssZZ");

    DateAndTime output;

    EXPECT_FALSE(apply_parser(compiled, "2026-04-13T17:42:58+02", output));
}

TEST(TimestampParserTest, ParsesBasicIso8601Utc)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYYMMDDThhmmssZ");

    DateAndTime output;

    EXPECT_TRUE(apply_parser(compiled, "20260413T174258Z", output));
    EXPECT_EQ(output.year, 2026);
    EXPECT_EQ(output.month, 4U);
    EXPECT_EQ(output.day, 13U);
    EXPECT_EQ(output.hour, 17U);
    EXPECT_EQ(output.minute, 42U);
    EXPECT_EQ(output.second, 58U);
    ASSERT_TRUE(output.utc_offset_minutes.has_value());
    EXPECT_EQ(*output.utc_offset_minutes, 0);
}

TEST(TimestampParserTest, ParsesBasicIso8601WithOffset)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYYMMDDThhmmssZZ");

    DateAndTime output;

    EXPECT_TRUE(apply_parser(compiled, "20260413T174258+0200", output));
    ASSERT_TRUE(output.utc_offset_minutes.has_value());
    EXPECT_EQ(*output.utc_offset_minutes, 120);
}

TEST(TimestampParserTest, ParsesCommaSeparatedMilliseconds)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY-MM-DD hh:mm:ss,fff");

    DateAndTime output;

    EXPECT_TRUE(apply_parser(compiled, "2026-04-13 17:42:58,123", output));
    EXPECT_EQ(output.nanosecond, 123000000U);
}

TEST(TimestampParserTest, ParsesAbbreviatedMonthName)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("DD-MMM-YYYY hh:mm:ss");

    DateAndTime output;

    EXPECT_TRUE(apply_parser(compiled, "13-Apr-2026 17:42:58", output));
    EXPECT_EQ(output.year, 2026);
    EXPECT_EQ(output.month, 4U);
    EXPECT_EQ(output.day, 13U);
    EXPECT_EQ(output.hour, 17U);
    EXPECT_EQ(output.minute, 42U);
    EXPECT_EQ(output.second, 58U);
}

TEST(TimestampParserTest, ParsesAbbreviatedMonthNameCaseInsensitive)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("DD-MMM-YYYY hh:mm:ss");

    DateAndTime output;

    EXPECT_TRUE(apply_parser(compiled, "13-aPr-2026 17:42:58", output));
    EXPECT_EQ(output.month, 4U);
}

TEST(TimestampParserTest, RejectsUnknownMonthAbbreviation)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("DD-MMM-YYYY hh:mm:ss");

    DateAndTime output;

    EXPECT_FALSE(apply_parser(compiled, "13-Axx-2026 17:42:58", output));
}

TEST(TimestampParserTest, ParsesSyslogStyleTimestampWithAbbreviatedMonth)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("MMM DD hh:mm:ss");

    DateAndTime output;

    EXPECT_TRUE(apply_parser(compiled, "Apr 13 17:42:58", output));
    EXPECT_EQ(output.month, 4U);
    EXPECT_EQ(output.day, 13U);
    EXPECT_EQ(output.hour, 17U);
    EXPECT_EQ(output.minute, 42U);
    EXPECT_EQ(output.second, 58U);
}

TEST(TimestampParserTest, ParsesApacheStyleTimestamp)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("DD/MMM/YYYY:hh:mm:ss ZZ");

    DateAndTime output;

    EXPECT_TRUE(apply_parser(compiled, "13/Apr/2026:17:42:58 +0200", output));
    EXPECT_EQ(output.year, 2026);
    EXPECT_EQ(output.month, 4U);
    EXPECT_EQ(output.day, 13U);
    EXPECT_EQ(output.hour, 17U);
    EXPECT_EQ(output.minute, 42U);
    EXPECT_EQ(output.second, 58U);
    ASSERT_TRUE(output.utc_offset_minutes.has_value());
    EXPECT_EQ(*output.utc_offset_minutes, 120);
}

TEST(TimestampParserTest, RejectsInvalidDayForMonth)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY-MM-DD hh:mm:ss");

    DateAndTime output;

    EXPECT_FALSE(apply_parser(compiled, "2026-04-31 17:42:58", output));
}

TEST(TimestampParserTest, RejectsNonLeapYearFebruary29)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY-MM-DD hh:mm:ss");

    DateAndTime output;

    EXPECT_FALSE(apply_parser(compiled, "2025-02-29 17:42:58", output));
}

TEST(TimestampParserTest, ParsesLeapYearFebruary29)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY-MM-DD hh:mm:ss");

    DateAndTime output;

    EXPECT_TRUE(apply_parser(compiled, "2024-02-29 17:42:58", output));
    EXPECT_EQ(output.year, 2024);
    EXPECT_EQ(output.month, 2U);
    EXPECT_EQ(output.day, 29U);
}

TEST(TimestampParserTest, AllowsTrailingGarbageAfterParsedTimestamp)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY-MM-DD hh:mm:ss");

    DateAndTime output;

    EXPECT_TRUE(apply_parser(compiled, "2026-04-13 17:42:58 abc", output));
    EXPECT_EQ(output.year, 2026);
    EXPECT_EQ(output.month, 4U);
    EXPECT_EQ(output.day, 13U);
    EXPECT_EQ(output.hour, 17U);
    EXPECT_EQ(output.minute, 42U);
    EXPECT_EQ(output.second, 58U);
}

TEST(TimestampParserTest, DetectsTimestampWhenNotAtStartOfString)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY-MM-DD hh:mm:ss");

    DateAndTime output;
    int match_start = -1;
    int match_end   = -1;

    EXPECT_TRUE(find_parser_match(compiled, "INFO 2026-04-13 17:42:58 abc", output, &match_start, &match_end));
    EXPECT_EQ(match_start, 5);
    EXPECT_EQ(match_end, 24);
    EXPECT_EQ(output.year, 2026);
    EXPECT_EQ(output.month, 4U);
    EXPECT_EQ(output.day, 13U);
    EXPECT_EQ(output.hour, 17U);
    EXPECT_EQ(output.minute, 42U);
    EXPECT_EQ(output.second, 58U);
}

TEST(TimestampParserTest, RejectsIncompleteTimestampWhenNotAtStartOfString)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY-MM-DD hh:mm:ss");

    DateAndTime output;

    EXPECT_FALSE(find_parser_match(compiled, "INFO 2026-04-13 17:42 abc", output));
}

TEST(TimestampParserTest, RejectsInvalidTimestampWhenNotAtStartOfString)
{
    TimestampParser parser;
    const auto compiled = parser.CompileFormat("YYYY-MM-DD hh:mm:ss");

    DateAndTime output;

    EXPECT_FALSE(find_parser_match(compiled, "INFO 2026-02-30 17:42:58 abc", output));
}

} // namespace eestv
