#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <utility>

#include "log_batch.hpp"
#include "timestamp/log_timestamp.hpp"

namespace slayerlog
{

namespace
{

LogBatchEntry make_entry(std::size_t source_index, std::string source_label, std::string text)
{
    const auto timestamp = parse_log_timestamp(text);
    return LogBatchEntry {
        source_index, std::move(source_label), std::move(text), timestamp, 0,
    };
}

} // namespace

TEST(LogBatchTest, PlacesUnsortableLinesBeforeTimestampedLines)
{
    const auto merged = merge_log_batch({
        make_entry(0, "alpha.log", "plain alpha"),
        make_entry(0, "alpha.log", "2026-04-01T10:02:00 alpha timed"),
        make_entry(1, "beta.log", "plain beta"),
        make_entry(1, "beta.log", "2026-04-01T10:01:00 beta timed"),
    });

    ASSERT_EQ(merged.size(), 4U);
    EXPECT_EQ(merged[0].text, "plain alpha");
    EXPECT_EQ(merged[1].text, "plain beta");
    EXPECT_EQ(merged[2].text, "2026-04-01T10:01:00 beta timed");
    EXPECT_EQ(merged[3].text, "2026-04-01T10:02:00 alpha timed");
}

TEST(LogBatchTest, SortsTimestampedLinesAcrossFilesByParsedTime)
{
    const auto merged = merge_log_batch({
        make_entry(0, "alpha.log", "2026-04-01T10:03:00 alpha third"),
        make_entry(0, "alpha.log", "2026-04-01T10:05:00 alpha fifth"),
        make_entry(1, "beta.log", "2026-04-01T10:04:00 beta fourth"),
    });

    ASSERT_EQ(merged.size(), 3U);
    EXPECT_EQ(merged[0].text, "2026-04-01T10:03:00 alpha third");
    EXPECT_EQ(merged[1].text, "2026-04-01T10:04:00 beta fourth");
    EXPECT_EQ(merged[2].text, "2026-04-01T10:05:00 alpha fifth");
}

TEST(LogBatchTest, KeepsOriginalSourceOrderForEqualTimestamps)
{
    const auto merged = merge_log_batch({
        make_entry(0, "alpha.log", "2026-04-01T10:00:00 alpha first"),
        make_entry(1, "beta.log", "2026-04-01T10:00:00 beta second"),
    });

    ASSERT_EQ(merged.size(), 2U);
    EXPECT_EQ(merged[0].text, "2026-04-01T10:00:00 alpha first");
    EXPECT_EQ(merged[1].text, "2026-04-01T10:00:00 beta second");
}

TEST(LogBatchTest, EmitsUntimestampedLinesWhenTheyReachTheFront)
{
    const auto merged = merge_log_batch({
        make_entry(0, "alpha.log", "2026-04-01T10:02:00 alpha timed"),
        make_entry(0, "alpha.log", "plain alpha first follow-up"),
        make_entry(0, "alpha.log", "plain alpha second follow-up"),
        make_entry(0, "alpha.log", "2026-04-01T10:05:00 alpha later"),
        make_entry(1, "beta.log", "2026-04-01T10:03:00 beta timed"),
    });

    ASSERT_EQ(merged.size(), 5U);
    EXPECT_EQ(merged[0].text, "2026-04-01T10:02:00 alpha timed");
    EXPECT_EQ(merged[1].text, "plain alpha first follow-up");
    EXPECT_EQ(merged[2].text, "plain alpha second follow-up");
    EXPECT_EQ(merged[3].text, "2026-04-01T10:03:00 beta timed");
    EXPECT_EQ(merged[4].text, "2026-04-01T10:05:00 alpha later");
}

} // namespace slayerlog
