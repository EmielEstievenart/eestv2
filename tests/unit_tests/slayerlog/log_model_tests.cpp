#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "tracked_sources/all_processed_sources.hpp"

namespace slayerlog
{

using LogModel = AllProcessedSources;

namespace
{

std::vector<std::string> rendered_texts(const LogModel& model)
{
    std::vector<std::string> lines;
    lines.reserve(static_cast<std::size_t>(model.line_count()));
    for (int index = 0; index < model.line_count(); ++index)
    {
        const auto rendered   = model.rendered_line(index);
        const auto prefix_end = rendered.find(' ');
        lines.push_back(rendered.substr(prefix_end + 1));
    }

    return lines;
}

std::vector<LogEntry> numbered_lines(int count)
{
    std::vector<LogEntry> lines;
    lines.reserve(static_cast<std::size_t>(count));
    for (int index = 1; index <= count; ++index)
    {
        lines.push_back({"alpha.log", "line " + std::to_string(index)});
    }

    return lines;
}

LogEntry make_batch_entry(std::size_t source_index, std::string source_label, std::string text, std::uint64_t sequence_number,
                         std::optional<std::chrono::system_clock::time_point> timestamp = std::nullopt, std::string parsed_time_text = {})
{
    LogEntry entry;
    entry.metadata.source_index  = source_index;
    entry.metadata.source_label  = std::move(source_label);
    entry.text                   = std::move(text);
    entry.metadata.timestamp     = timestamp;
    entry.metadata.sequence_number = sequence_number;
    entry.metadata.parsed_time_text = std::move(parsed_time_text);
    return entry;
}

} // namespace

TEST(LogModelTest, AppendsLinesInProvidedOrder)
{
    LogModel model;

    model.append_lines({
        LogEntry {"alpha.log", "plain alpha"},
        LogEntry {"beta.log", "2026-04-01T10:01:00 beta timed"},
        LogEntry {"alpha.log", "2026-04-01T10:02:00 alpha timed"},
    });

    EXPECT_EQ(rendered_texts(model), (std::vector<std::string> {
                                         "plain alpha",
                                         "2026-04-01T10:01:00 beta timed",
                                         "2026-04-01T10:02:00 alpha timed",
                                     }));
}

TEST(LogModelTest, PausedUpdatesAppendWhenResumed)
{
    LogModel model;
    model.toggle_pause();

    model.append_lines({
        LogEntry {"alpha.log", "first"},
        LogEntry {"beta.log", "second"},
    });

    EXPECT_EQ(model.line_count(), 0);

    model.toggle_pause();

    EXPECT_EQ(rendered_texts(model), (std::vector<std::string> {
                                         "first",
                                         "second",
                                     }));
}

TEST(LogModelTest, ReplaceBatchPreservesSessionState)
{
    LogModel model;
    model.set_show_source_labels(true);
    model.append_lines({
        LogEntry {"alpha.log", "error seed"},
    });
    model.add_include_filter("error");
    model.hide_before_line_number(2);
    model.hide_columns(2, 4);
    model.toggle_pause();

    model.replace_batch({
        make_batch_entry(0, "alpha.log", "info one", 0),
        make_batch_entry(0, "alpha.log", "error two", 1),
    });

    EXPECT_TRUE(model.updates_paused());
    EXPECT_EQ(model.hidden_before_line_number(), 2);
    ASSERT_TRUE(model.hidden_columns().has_value());
    EXPECT_EQ(*model.hidden_columns(), (HiddenColumnRange {2, 4}));
    EXPECT_EQ(model.line_count(), 1);

    model.append_batch({
        make_batch_entry(0, "alpha.log", "error three", 2),
    });
    EXPECT_EQ(model.line_count(), 1);

    model.toggle_pause();
    EXPECT_EQ(model.line_count(), 2);
}

TEST(LogModelTest, ResetClearsAllLoadedAndDerivedState)
{
    LogModel model;
    model.set_show_source_labels(true);

    model.append_lines({
        LogEntry {"alpha.log", "error one"},
        LogEntry {"alpha.log", "info two"},
    });
    model.add_include_filter("error");
    model.add_exclude_filter("ignore");
    model.hide_before_line_number(2);
    model.hide_columns(2, 5);
    model.toggle_pause();
    model.append_lines({
        LogEntry {"alpha.log", "queued while paused"},
    });

    model.reset();

    EXPECT_EQ(model.line_count(), 0);
    EXPECT_EQ(model.total_line_count(), 0);
    EXPECT_TRUE(model.include_filters().empty());
    EXPECT_TRUE(model.exclude_filters().empty());
    EXPECT_FALSE(model.hidden_before_line_number().has_value());
    EXPECT_FALSE(model.hidden_columns().has_value());
    EXPECT_FALSE(model.updates_paused());

    model.append_lines({
        LogEntry {"alpha.log", "post-reset"},
    });
    EXPECT_EQ(model.rendered_line(0), "1 post-reset");
}

TEST(LogModelTest, RendersSourceLabelsWhenEnabled)
{
    LogModel model;
    model.set_show_source_labels(true);

    model.append_lines({
        LogEntry {"alpha.log", "hello"},
    });

    EXPECT_EQ(model.rendered_line(0), "1 [alpha.log] hello");
}

TEST(LogModelTest, RendersParsedTimeBeforeOriginalText)
{
    LogModel model;

    model.append_lines({
        LogEntry {"alpha.log", "INFO 2026-04-01 10:00:00 hello", std::nullopt, "2026-04-01 10:00:00"},
    });

    EXPECT_EQ(model.rendered_line(0), "1 {2026-04-01 10:00:00} INFO 2026-04-01 10:00:00 hello");
}

TEST(LogModelTest, RenderedLinesReturnsVisibleSlice)
{
    LogModel model;

    model.append_lines({
        LogEntry {"alpha.log", "first"},
        LogEntry {"alpha.log", "second"},
        LogEntry {"alpha.log", "third"},
    });
    model.add_exclude_filter("second");

    EXPECT_EQ(model.rendered_lines(0, 2), (std::vector<std::string> {
                                              "1 first",
                                              "3 third",
                                          }));
}

TEST(LogModelTest, FiltersApplyRetroactivelyAndToNewLines)
{
    LogModel model;

    model.append_lines({
        LogEntry {"alpha.log", "error critical"},
        LogEntry {"alpha.log", "warn noisy"},
        LogEntry {"alpha.log", "warn useful"},
        LogEntry {"alpha.log", "info ignored"},
    });

    model.add_include_filter("error");
    model.add_include_filter("warn");
    model.add_exclude_filter("noisy");

    EXPECT_EQ(rendered_texts(model), (std::vector<std::string> {
                                         "error critical",
                                         "warn useful",
                                     }));

    model.append_lines({
        LogEntry {"alpha.log", "warn newest"},
        LogEntry {"alpha.log", "debug newest"},
    });
    EXPECT_EQ(rendered_texts(model), (std::vector<std::string> {
                                         "error critical",
                                         "warn useful",
                                         "warn newest",
                                     }));

    model.reset_filters();
    EXPECT_EQ(model.line_count(), 6);
}

TEST(LogModelTest, HideBeforeLineUsesRawLineNumbers)
{
    LogModel model;
    model.append_lines(numbered_lines(5));

    model.hide_before_line_number(3);

    EXPECT_EQ(rendered_texts(model), (std::vector<std::string> {
                                         "line 3",
                                         "line 4",
                                         "line 5",
                                     }));
    EXPECT_EQ(model.hidden_before_line_number(), 3);
    EXPECT_EQ(model.line_number_for_visible_line(VisibleLineIndex {0}), 3);
    EXPECT_EQ(model.line_number_for_visible_line(VisibleLineIndex {2}), 5);
    EXPECT_FALSE(model.line_number_for_visible_line(VisibleLineIndex {3}).has_value());
    ASSERT_TRUE(model.visible_line_index_for_line_number(3).has_value());
    EXPECT_EQ(model.visible_line_index_for_line_number(3)->value, 0);
    EXPECT_FALSE(model.visible_line_index_for_line_number(2).has_value());
}

TEST(LogModelTest, HideColumnsRemovesDisplayedRange)
{
    LogModel model;
    model.append_lines({
        LogEntry {"alpha.log", "abcdef"},
    });

    model.hide_columns(2, 4);

    ASSERT_TRUE(model.hidden_columns().has_value());
    EXPECT_EQ(*model.hidden_columns(), (HiddenColumnRange {2, 4}));
    EXPECT_EQ(model.rendered_line(0), "1 cdef");
}

TEST(LogModelTest, ResetHiddenColumnsRestoresRenderedText)
{
    LogModel model;
    model.append_lines({
        LogEntry {"alpha.log", "abcdef"},
    });
    model.hide_columns(2, 4);

    model.reset_hidden_columns();

    EXPECT_FALSE(model.hidden_columns().has_value());
    EXPECT_EQ(model.rendered_line(0), "1 abcdef");
}

TEST(LogModelTest, ParseHiddenColumnRangeUsesHalfOpenZeroBasedSyntax)
{
    const auto trimmed_range = parse_hidden_column_range(" 4 - 10 ");
    ASSERT_TRUE(trimmed_range.has_value());
    EXPECT_EQ(*trimmed_range, (HiddenColumnRange {4, 10}));
    EXPECT_FALSE(parse_hidden_column_range("4-4").has_value());
    EXPECT_FALSE(parse_hidden_column_range("-1-4").has_value());
    EXPECT_FALSE(parse_hidden_column_range("4-ten").has_value());
}

TEST(LogModelTest, FiltersSupportRegexWithRePrefix)
{
    LogModel model;
    model.append_lines({
        LogEntry {"alpha.log", "status=200 ok"},
        LogEntry {"alpha.log", "status=404 missing"},
        LogEntry {"alpha.log", "status=500 error"},
    });

    model.add_include_filter("re:status=(4|5)\\d\\d");
    model.add_exclude_filter("re:status=404");

    EXPECT_EQ(rendered_texts(model), (std::vector<std::string> {
                                         "status=500 error",
                                     }));
}

TEST(LogModelTest, InvalidRegexFilterIsRejectedAndKeepsExistingFilters)
{
    LogModel model;
    model.append_lines({
        LogEntry {"alpha.log", "error first"},
        LogEntry {"alpha.log", "warn second"},
    });

    model.add_include_filter("error");
    EXPECT_EQ(rendered_texts(model), (std::vector<std::string> {
                                         "error first",
                                     }));

    EXPECT_THROW(model.add_include_filter("re:["), std::invalid_argument);
    EXPECT_THROW(model.add_exclude_filter("re:"), std::invalid_argument);

    EXPECT_EQ(model.include_filters().size(), 1U);
    EXPECT_EQ(model.exclude_filters().size(), 0U);
    EXPECT_EQ(rendered_texts(model), (std::vector<std::string> {
                                         "error first",
                                     }));
}

} // namespace slayerlog
