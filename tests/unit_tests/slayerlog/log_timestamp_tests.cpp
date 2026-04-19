#include <gtest/gtest.h>

#include "timestamp/log_timestamp.hpp"

namespace slayerlog
{

TEST(LogTimestampTest, ParsesBracketedIsoTimestamp)
{
    const auto parsed = parse_log_timestamp("[2026-04-01T12:34:56] hello");
    EXPECT_TRUE(parsed.has_value());
}

TEST(LogTimestampTest, ParsesSpaceSeparatedTimestampWithFraction)
{
    const auto parsed = parse_log_timestamp("2026-04-01 12:34:56.123 details");
    EXPECT_TRUE(parsed.has_value());
}

TEST(LogTimestampTest, NormalizesZuluAndOffsetTimestamps)
{
    const auto zulu   = parse_log_timestamp("2026-04-01T10:34:56Z event");
    const auto offset = parse_log_timestamp("2026-04-01T12:34:56+0200 event");

    ASSERT_TRUE(zulu.has_value());
    ASSERT_TRUE(offset.has_value());
    EXPECT_EQ(*zulu, *offset);
}

TEST(LogTimestampTest, ParsesColonSeparatedTimezoneOffset)
{
    const auto earlier = parse_log_timestamp("2026-04-01T12:34:56+02:00 first");
    const auto later   = parse_log_timestamp("2026-04-01T12:35:56+02:00 second");

    ASSERT_TRUE(earlier.has_value());
    ASSERT_TRUE(later.has_value());
    EXPECT_LT(*earlier, *later);
}

TEST(LogTimestampTest, RejectsUnsupportedStrings)
{
    EXPECT_FALSE(parse_log_timestamp("INFO no timestamp here").has_value());
    EXPECT_FALSE(parse_log_timestamp("12:34:56 time only").has_value());
    EXPECT_FALSE(parse_log_timestamp("[2026-04-01T12:34:56 missing bracket").has_value());
}

TEST(LogTimestampTest, DetectsTimestampAfterPrefixAndKeepsCompiledParser)
{
    auto formats = std::make_shared<const TimestampFormatCatalog>(std::vector<std::string> {"YYYY-MM-DD hh:mm:ss"});
    SourceTimestampParser parser(formats);
    RawLogLine first_line("INFO 2026-04-01 12:34:56 first");
    RawLogLine second_line("WARN 2026-04-01 12:35:56 second");

    const bool first_parsed  = parser.parse(first_line);
    const bool second_parsed = parser.parse(second_line);

    ASSERT_TRUE(first_parsed);
    ASSERT_TRUE(second_parsed);
    EXPECT_EQ(first_line.metadata.extracted_time_text, "2026-04-01 12:34:56");
    EXPECT_EQ(second_line.metadata.extracted_time_text, "2026-04-01 12:35:56");
    EXPECT_EQ(first_line.metadata.parsed_time_text, "2026-04-01 12:34:56");
    EXPECT_EQ(second_line.metadata.parsed_time_text, "2026-04-01 12:35:56");
}

TEST(LogTimestampTest, FormatsTimezoneInDisplayText)
{
    const auto parsed = parse_log_timestamp_details("2026-04-01T12:34:56+0200 event");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->parsed_time_text, "2026-04-01 12:34:56+02:00");
}

} // namespace slayerlog
