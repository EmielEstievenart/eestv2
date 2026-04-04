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

TEST(LogViewModelTest, IncludeFiltersApplyRetroactively)
{
    LogViewModel model;

    model.append_lines({
        ObservedLogLine {"alpha.log", "debug one"},
        ObservedLogLine {"alpha.log", "error two"},
        ObservedLogLine {"alpha.log", "warn three"},
    });

    model.add_include_filter("error");

    EXPECT_EQ(rendered_texts(model), (std::vector<std::string> {"error two"}));
    EXPECT_EQ(model.rendered_line(0), "2 error two");
}

TEST(LogViewModelTest, MultipleIncludeAndExcludeFiltersWorkTogether)
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
}

TEST(LogViewModelTest, ResetFiltersRestoresPreviouslyHiddenLines)
{
    LogViewModel model;

    model.append_lines({
        ObservedLogLine {"alpha.log", "first"},
        ObservedLogLine {"alpha.log", "second"},
    });

    model.add_include_filter("second");
    ASSERT_EQ(rendered_texts(model), (std::vector<std::string> {"second"}));

    model.reset_filters();

    EXPECT_EQ(rendered_texts(model), (std::vector<std::string> {
                                         "first",
                                         "second",
                                     }));
}

TEST(LogViewModelTest, ActiveFiltersApplyToNewlyAppendedLines)
{
    LogViewModel model;
    model.add_include_filter("error");

    model.append_lines({
        ObservedLogLine {"alpha.log", "info first"},
        ObservedLogLine {"alpha.log", "error second"},
    });

    EXPECT_EQ(rendered_texts(model), (std::vector<std::string> {"error second"}));
}

TEST(LogViewModelTest, PausedUpdatesRespectActiveFiltersWhenFlushed)
{
    LogViewModel model;
    model.add_exclude_filter("skip");
    model.toggle_pause();

    model.append_lines({
        ObservedLogLine {"alpha.log", "keep this"},
        ObservedLogLine {"alpha.log", "skip this"},
    });

    EXPECT_EQ(model.line_count(), 0);

    model.toggle_pause();

    EXPECT_EQ(rendered_texts(model), (std::vector<std::string> {"keep this"}));
}

} // namespace slayerlog
