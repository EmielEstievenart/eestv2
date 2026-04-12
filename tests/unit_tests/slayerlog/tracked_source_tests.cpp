#include <gtest/gtest.h>

#include "tracked_source.hpp"

namespace slayerlog
{

TEST(TrackedSourceTest, StoresParsedEntriesAndSequenceNumbers)
{
    TrackedSource tracked_source(parse_log_source("alpha.log"), "alpha.log");

    tracked_source.add_entries_from_raw_strings({
        "2026-04-01T10:00:00 first",
        "plain second",
    });

    const auto& entries = tracked_source.entries();
    ASSERT_EQ(entries.size(), 2U);

    EXPECT_EQ(entries[0].raw_text, "2026-04-01T10:00:00 first");
    EXPECT_TRUE(entries[0].timestamp.has_value());
    EXPECT_EQ(entries[0].extracted_timestamp_text, "2026-04-01T10:00:00");
    EXPECT_EQ(entries[0].sequence_number, 0U);

    EXPECT_EQ(entries[1].raw_text, "plain second");
    EXPECT_FALSE(entries[1].timestamp.has_value());
    EXPECT_TRUE(entries[1].extracted_timestamp_text.empty());
    EXPECT_EQ(entries[1].sequence_number, 1U);
}

TEST(TrackedSourceTest, UpdatesSourceLabelWithoutTouchingStoredEntries)
{
    TrackedSource tracked_source(parse_log_source("alpha.log"), "alpha.log");
    tracked_source.add_entry_from_raw_string("plain line");

    tracked_source.set_source_label("renamed.log");

    EXPECT_EQ(tracked_source.source_label(), "renamed.log");
    ASSERT_EQ(tracked_source.entries().size(), 1U);
    EXPECT_EQ(tracked_source.entries()[0].raw_text, "plain line");
}

} // namespace slayerlog
