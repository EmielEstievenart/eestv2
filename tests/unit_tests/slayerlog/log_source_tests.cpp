#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "log_source.hpp"

namespace slayerlog
{

TEST(LogSourceTest, ParsesLocalPathAsLocalFile)
{
    const LogSource source = parse_log_source("logs/app.log");

    EXPECT_EQ(source.kind, LogSourceKind::LocalFile);
    EXPECT_EQ(source.local_path, "logs/app.log");
    EXPECT_EQ(source_display_path(source), "logs/app.log");
    EXPECT_EQ(source_basename(source), "app.log");
}

TEST(LogSourceTest, ParsesSshSource)
{
    const LogSource source = parse_log_source("ssh://user@example.com/var/log/app.log");

    EXPECT_EQ(source.kind, LogSourceKind::SshRemoteFile);
    EXPECT_EQ(source.ssh_target, "user@example.com");
    EXPECT_EQ(source.remote_path, "/var/log/app.log");
    EXPECT_EQ(source_display_path(source), "ssh://user@example.com/var/log/app.log");
    EXPECT_EQ(source_basename(source), "app.log");
}

TEST(LogSourceTest, RejectsRemoteSourceWithoutAbsolutePath)
{
    EXPECT_THROW(parse_log_source("ssh://example.com"), std::invalid_argument);
    EXPECT_THROW(parse_log_source("ssh://example.com/"), std::invalid_argument);
}

TEST(LogSourceTest, MatchesEquivalentRemoteSources)
{
    const LogSource left  = parse_log_source("ssh://user@EXAMPLE.com/var/log/../log/app.log");
    const LogSource right = parse_log_source("ssh://user@example.com/var/log/app.log");

    EXPECT_TRUE(same_source(left, right));
}

TEST(LogSourceTest, UsesFullSourceWhenBasenameCollides)
{
    const std::vector<LogSource> sources {
        parse_log_source("first/app.log"),
        parse_log_source("ssh://user@example.com/var/log/app.log"),
    };

    const auto labels = build_source_labels(sources);
    ASSERT_EQ(labels.size(), 2U);
    EXPECT_EQ(labels[0], "first/app.log");
    EXPECT_EQ(labels[1], "ssh://user@example.com/var/log/app.log");
}

} // namespace slayerlog
