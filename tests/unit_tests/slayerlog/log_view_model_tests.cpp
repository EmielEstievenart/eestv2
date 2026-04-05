#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "log_view_model.hpp"

namespace slayerlog
{

namespace
{

std::vector<std::string> rendered_texts(const LogViewModel& model)
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

std::vector<ObservedLogLine> numbered_lines(int count)
{
    std::vector<ObservedLogLine> lines;
    lines.reserve(static_cast<std::size_t>(count));
    for (int index = 1; index <= count; ++index)
    {
        lines.push_back({"alpha.log", "line " + std::to_string(index)});
    }

    return lines;
}

} // namespace

TEST(LogViewModelTest, AppendsLinesInProvidedOrder)
{
    LogViewModel model;

    model.append_lines({
        ObservedLogLine {"alpha.log", "plain alpha"},
        ObservedLogLine {"beta.log", "2026-04-01T10:01:00 beta timed"},
        ObservedLogLine {"alpha.log", "2026-04-01T10:02:00 alpha timed"},
    });

    EXPECT_EQ(rendered_texts(model), (std::vector<std::string> {
                                         "plain alpha",
                                         "2026-04-01T10:01:00 beta timed",
                                         "2026-04-01T10:02:00 alpha timed",
                                     }));
}

TEST(LogViewModelTest, PausedUpdatesAppendWhenResumed)
{
    LogViewModel model;
    model.toggle_pause();

    model.append_lines({
        ObservedLogLine {"alpha.log", "first"},
        ObservedLogLine {"beta.log", "second"},
    });

    EXPECT_EQ(model.line_count(), 0);

    model.toggle_pause();

    EXPECT_EQ(rendered_texts(model), (std::vector<std::string> {
                                         "first",
                                         "second",
                                     }));
}

TEST(LogViewModelTest, RendersSourceLabelsWhenEnabled)
{
    LogViewModel model;
    model.set_show_source_labels(true);

    model.append_lines({
        ObservedLogLine {"alpha.log", "hello"},
    });

    EXPECT_EQ(model.rendered_line(0), "1 [alpha.log] hello");
}

TEST(LogViewModelTest, RenderedLinesReturnsVisibleSlice)
{
    LogViewModel model;

    model.append_lines({
        ObservedLogLine {"alpha.log", "first"},
        ObservedLogLine {"alpha.log", "second"},
        ObservedLogLine {"alpha.log", "third"},
    });
    model.add_exclude_filter("second");

    EXPECT_EQ(model.rendered_lines(0, 2), (std::vector<std::string> {
                                              "1 first",
                                              "3 third",
                                          }));
}

TEST(LogViewModelTest, FiltersApplyRetroactivelyAndToNewLines)
{
    LogViewModel model;

    model.append_lines({
        ObservedLogLine {"alpha.log", "error critical"},
        ObservedLogLine {"alpha.log", "warn noisy"},
        ObservedLogLine {"alpha.log", "warn useful"},
        ObservedLogLine {"alpha.log", "info ignored"},
    });

    model.add_include_filter("error");
    model.add_include_filter("warn");
    model.add_exclude_filter("noisy");

    EXPECT_EQ(rendered_texts(model), (std::vector<std::string> {
                                         "error critical",
                                         "warn useful",
                                     }));

    model.append_lines({
        ObservedLogLine {"alpha.log", "warn newest"},
        ObservedLogLine {"alpha.log", "debug newest"},
    });
    EXPECT_EQ(rendered_texts(model), (std::vector<std::string> {
                                         "error critical",
                                         "warn useful",
                                         "warn newest",
                                     }));

    model.reset_filters();
    EXPECT_EQ(model.line_count(), 6);
}

TEST(LogViewModelTest, HideBeforeLineUsesRawLineNumbers)
{
    LogViewModel model;
    model.append_lines(numbered_lines(5));

    model.hide_before_line_number(3);

    EXPECT_EQ(rendered_texts(model), (std::vector<std::string> {
                                         "line 3",
                                         "line 4",
                                         "line 5",
                                     }));
    EXPECT_EQ(model.hidden_before_line_number(), 3);
    ASSERT_TRUE(model.visible_line_index_for_line_number(3).has_value());
    EXPECT_EQ(model.visible_line_index_for_line_number(3)->value, 0);
    EXPECT_FALSE(model.visible_line_index_for_line_number(2).has_value());
}

TEST(LogViewModelTest, FindQueryBuildsMatchIndexesAndLookupHelpers)
{
    LogViewModel model;
    model.append_lines({
        ObservedLogLine {"alpha.log", "error first"},
        ObservedLogLine {"alpha.log", "info second"},
        ObservedLogLine {"alpha.log", "error third"},
    });

    EXPECT_TRUE(model.set_find_query("error"));
    EXPECT_TRUE(model.find_active());
    EXPECT_EQ(model.total_find_match_count(), 2);
    EXPECT_EQ(model.visible_find_match_count(), 2);

    ASSERT_TRUE(model.find_match_entry_index(FindResultIndex {0}).has_value());
    EXPECT_EQ(*model.find_match_entry_index(FindResultIndex {0}), (AllLineIndex {0}));
    ASSERT_TRUE(model.find_match_entry_index(FindResultIndex {1}).has_value());
    EXPECT_EQ(*model.find_match_entry_index(FindResultIndex {1}), (AllLineIndex {2}));

    ASSERT_TRUE(model.find_match_position_for_entry_index(AllLineIndex {2}).has_value());
    EXPECT_EQ(model.find_match_position_for_entry_index(AllLineIndex {2})->value, 1);

    ASSERT_TRUE(model.visible_line_index_for_entry(AllLineIndex {2}).has_value());
    EXPECT_EQ(model.visible_line_index_for_entry(AllLineIndex {2})->value, 2);
    ASSERT_TRUE(model.visible_line_index_for_line_number(1).has_value());
    EXPECT_EQ(model.visible_line_index_for_line_number(1)->value, 0);

    EXPECT_TRUE(model.visible_line_matches_find(0));
    EXPECT_FALSE(model.visible_line_matches_find(1));
    EXPECT_TRUE(model.visible_line_matches_find(2));
}

TEST(LogViewModelTest, FindCountsAllMatchesWhileVisibleCountRespectsFilters)
{
    LogViewModel model;
    model.append_lines({
        ObservedLogLine {"alpha.log", "error first"},
        ObservedLogLine {"alpha.log", "error hidden"},
        ObservedLogLine {"alpha.log", "error third"},
    });
    model.add_exclude_filter("hidden");

    EXPECT_TRUE(model.set_find_query("error"));
    EXPECT_EQ(model.total_find_match_count(), 3);
    EXPECT_EQ(model.visible_find_match_count(), 2);
    EXPECT_FALSE(model.visible_line_index_for_line_number(2).has_value());
}

TEST(LogViewModelTest, ClearingFindQueryRemovesAllFindState)
{
    LogViewModel model;
    model.append_lines({
        ObservedLogLine {"alpha.log", "needle"},
    });

    ASSERT_TRUE(model.set_find_query("needle"));
    ASSERT_TRUE(model.find_active());

    model.clear_find_query();

    EXPECT_FALSE(model.find_active());
    EXPECT_EQ(model.total_find_match_count(), 0);
    EXPECT_EQ(model.visible_find_match_count(), 0);
}

} // namespace slayerlog
