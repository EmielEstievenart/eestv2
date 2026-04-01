#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

#include "log_view_model.hpp"

namespace slayerlog
{

namespace
{

ObservedLogUpdate make_update(
    std::string source_path,
    std::size_t source_index,
    FileWatcher::Update::Kind kind,
    std::vector<std::string> lines,
    bool is_initial_load,
    std::uint64_t poll_epoch = 0)
{
    ObservedLogUpdate update;
    update.source_path     = std::move(source_path);
    update.source_label    = update.source_path;
    update.kind            = kind;
    update.lines           = std::move(lines);
    update.source_index    = source_index;
    update.is_initial_load = is_initial_load;
    update.poll_epoch      = poll_epoch;
    return update;
}

std::vector<std::string> rendered_texts(const LogViewModel& model)
{
    std::vector<std::string> lines;
    lines.reserve(static_cast<std::size_t>(model.line_count()));
    for (int index = 0; index < model.line_count(); ++index)
    {
        const auto rendered = model.rendered_line(index);
        const auto prefix_end = rendered.find(' ');
        lines.push_back(rendered.substr(prefix_end + 1));
    }

    return lines;
}

} // namespace

TEST(LogViewModelTest, MergesInitialTimestampedFilesByParsedTime)
{
    LogViewModel model;
    model.apply_initial_updates({
        make_update(
            "alpha.log",
            0,
            FileWatcher::Update::Kind::Snapshot,
            {"2026-04-01T10:00:00 alpha one", "2026-04-01T10:02:00 alpha two"},
            true),
        make_update(
            "beta.log",
            1,
            FileWatcher::Update::Kind::Snapshot,
            {"2026-04-01T10:01:00 beta one"},
            true),
    });

    EXPECT_EQ(
        rendered_texts(model),
        (std::vector<std::string>{
            "2026-04-01T10:00:00 alpha one",
            "2026-04-01T10:01:00 beta one",
            "2026-04-01T10:02:00 alpha two",
        }));
}

TEST(LogViewModelTest, KeepsUntimestampedInitialFilesAsTrailingBlocks)
{
    LogViewModel model;
    model.apply_initial_updates({
        make_update(
            "plain.log",
            0,
            FileWatcher::Update::Kind::Snapshot,
            {"plain one", "plain two"},
            true),
        make_update(
            "timed.log",
            1,
            FileWatcher::Update::Kind::Snapshot,
            {"2026-04-01T10:00:00 timed one"},
            true),
    });

    EXPECT_EQ(
        rendered_texts(model),
        (std::vector<std::string>{
            "2026-04-01T10:00:00 timed one",
            "plain one",
            "plain two",
        }));
}

TEST(LogViewModelTest, KeepsPrefixesAndContinuationLinesAttachedToTimestampGroups)
{
    LogViewModel model;
    model.apply_initial_updates({
        make_update(
            "alpha.log",
            0,
            FileWatcher::Update::Kind::Snapshot,
            {
                "alpha prefix",
                "2026-04-01T10:01:00 alpha event",
                "alpha continuation",
                "2026-04-01T10:03:00 alpha later",
            },
            true),
        make_update(
            "beta.log",
            1,
            FileWatcher::Update::Kind::Snapshot,
            {"2026-04-01T10:00:00 beta first", "2026-04-01T10:02:00 beta second"},
            true),
    });

    EXPECT_EQ(
        rendered_texts(model),
        (std::vector<std::string>{
            "2026-04-01T10:00:00 beta first",
            "alpha prefix",
            "2026-04-01T10:01:00 alpha event",
            "alpha continuation",
            "2026-04-01T10:02:00 beta second",
            "2026-04-01T10:03:00 alpha later",
        }));
}

TEST(LogViewModelTest, LiveAppendsStayAfterInitialContentAndSortWithinEpoch)
{
    LogViewModel model;
    model.apply_initial_updates({
        make_update(
            "alpha.log",
            0,
            FileWatcher::Update::Kind::Snapshot,
            {"2026-04-01T10:05:00 alpha initial"},
            true),
    });

    model.apply_update(make_update(
        "alpha.log",
        0,
        FileWatcher::Update::Kind::Append,
        {"2026-04-01T10:01:00 alpha append"},
        false,
        1));
    model.apply_update(make_update(
        "beta.log",
        1,
        FileWatcher::Update::Kind::Append,
        {"2026-04-01T10:02:00 beta append"},
        false,
        1));

    EXPECT_EQ(
        rendered_texts(model),
        (std::vector<std::string>{
            "2026-04-01T10:05:00 alpha initial",
            "2026-04-01T10:01:00 alpha append",
            "2026-04-01T10:02:00 beta append",
        }));
}

TEST(LogViewModelTest, KeepsUntimestampedAppendBlocksContiguousWithinEpoch)
{
    LogViewModel model;
    model.apply_initial_updates({
        make_update(
            "alpha.log",
            0,
            FileWatcher::Update::Kind::Snapshot,
            {"2026-04-01T10:00:00 alpha initial"},
            true),
    });

    model.apply_update(make_update(
        "beta.log",
        1,
        FileWatcher::Update::Kind::Append,
        {"2026-04-01T10:01:00 beta append"},
        false,
        1));
    model.apply_update(make_update(
        "alpha.log",
        0,
        FileWatcher::Update::Kind::Append,
        {"plain append one", "plain append two"},
        false,
        1));

    EXPECT_EQ(
        rendered_texts(model),
        (std::vector<std::string>{
            "2026-04-01T10:00:00 alpha initial",
            "2026-04-01T10:01:00 beta append",
            "plain append one",
            "plain append two",
        }));
}

TEST(LogViewModelTest, PreservesEpochOrderWhenPausedUpdatesResume)
{
    LogViewModel model;
    model.toggle_pause();

    model.apply_update(make_update(
        "alpha.log",
        0,
        FileWatcher::Update::Kind::Append,
        {"2026-04-01T10:02:00 epoch one"},
        false,
        1));
    model.apply_update(make_update(
        "alpha.log",
        0,
        FileWatcher::Update::Kind::Append,
        {"2026-04-01T09:59:00 epoch two"},
        false,
        2));

    model.toggle_pause();

    EXPECT_EQ(
        rendered_texts(model),
        (std::vector<std::string>{
            "2026-04-01T10:02:00 epoch one",
            "2026-04-01T09:59:00 epoch two",
        }));
}

TEST(LogViewModelTest, LiveSnapshotsReplacePriorSourceEntries)
{
    LogViewModel model;
    model.apply_initial_updates({
        make_update(
            "alpha.log",
            0,
            FileWatcher::Update::Kind::Snapshot,
            {"2026-04-01T10:00:00 alpha old", "2026-04-01T10:01:00 alpha old two"},
            true),
        make_update(
            "beta.log",
            1,
            FileWatcher::Update::Kind::Snapshot,
            {"2026-04-01T10:02:00 beta keep"},
            true),
    });

    model.apply_update(make_update(
        "alpha.log",
        0,
        FileWatcher::Update::Kind::Snapshot,
        {"2026-04-01T11:00:00 alpha reset"},
        false,
        3));

    EXPECT_EQ(
        rendered_texts(model),
        (std::vector<std::string>{
            "2026-04-01T10:02:00 beta keep",
            "2026-04-01T11:00:00 alpha reset",
        }));
}

} // namespace slayerlog
