#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "log_batch.hpp"

namespace slayerlog
{

#if !defined(NDEBUG)
namespace
{

void merge_log_batch_with_mismatched_source_labels()
{
    const std::vector<WatcherLineBatch> watcher_batches{
        WatcherLineBatch{
            "2026-04-01T10:02:00 alpha timed",
        },
    };
    const std::vector<std::string> source_labels;
    const auto merged = merge_log_batch(watcher_batches, source_labels);
    (void)merged;
}

} // namespace
#endif

TEST(LogBatchTest, PlacesUnsortableLinesBeforeTimestampedLines)
{
    const auto merged = merge_log_batch({
        WatcherLineBatch{
            "plain alpha",
            "2026-04-01T10:02:00 alpha timed",
        },
        WatcherLineBatch{
            "plain beta",
            "2026-04-01T10:01:00 beta timed",
        },
    }, {"alpha.log", "beta.log"});

    ASSERT_EQ(merged.size(), 4U);
    EXPECT_EQ(merged[0].text, "plain alpha");
    EXPECT_EQ(merged[1].text, "plain beta");
    EXPECT_EQ(merged[2].text, "2026-04-01T10:01:00 beta timed");
    EXPECT_EQ(merged[3].text, "2026-04-01T10:02:00 alpha timed");
}

TEST(LogBatchTest, SortsTimestampedLinesAcrossFilesByParsedTime)
{
    const auto merged = merge_log_batch({
        WatcherLineBatch{
            "2026-04-01T10:03:00 alpha third",
            "2026-04-01T10:05:00 alpha fifth",
        },
        WatcherLineBatch{
            "2026-04-01T10:04:00 beta fourth",
        },
    }, {"alpha.log", "beta.log"});

    ASSERT_EQ(merged.size(), 3U);
    EXPECT_EQ(merged[0].text, "2026-04-01T10:03:00 alpha third");
    EXPECT_EQ(merged[1].text, "2026-04-01T10:04:00 beta fourth");
    EXPECT_EQ(merged[2].text, "2026-04-01T10:05:00 alpha fifth");
}

TEST(LogBatchTest, KeepsOriginalOrderForEqualTimestamps)
{
    const auto merged = merge_log_batch({
        WatcherLineBatch{
            "2026-04-01T10:00:00 alpha first",
        },
        WatcherLineBatch{
            "2026-04-01T10:00:00 beta second",
        },
    }, {"alpha.log", "beta.log"});

    ASSERT_EQ(merged.size(), 2U);
    EXPECT_EQ(merged[0].text, "2026-04-01T10:00:00 alpha first");
    EXPECT_EQ(merged[1].text, "2026-04-01T10:00:00 beta second");
}

TEST(LogBatchTest, EmitsUntimestampedLinesWhenTheyReachTheFront)
{
    const auto merged = merge_log_batch({
        WatcherLineBatch{
            "2026-04-01T10:02:00 alpha timed",
            "plain alpha first follow-up",
            "plain alpha second follow-up",
            "2026-04-01T10:05:00 alpha later",
        },
        WatcherLineBatch{
            "2026-04-01T10:03:00 beta timed",
        },
    }, {"alpha.log", "beta.log"});

    ASSERT_EQ(merged.size(), 5U);
    EXPECT_EQ(merged[0].text, "2026-04-01T10:02:00 alpha timed");
    EXPECT_EQ(merged[1].text, "plain alpha first follow-up");
    EXPECT_EQ(merged[2].text, "plain alpha second follow-up");
    EXPECT_EQ(merged[3].text, "2026-04-01T10:03:00 beta timed");
    EXPECT_EQ(merged[4].text, "2026-04-01T10:05:00 alpha later");
}

#if !defined(NDEBUG)
TEST(LogBatchTest, AssertsWhenSourceLabelCountDoesNotMatchWatcherCount)
{
    EXPECT_DEATH(merge_log_batch_with_mismatched_source_labels(), "");
}
#endif

} // namespace slayerlog
