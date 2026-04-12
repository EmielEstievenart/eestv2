#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "log_batch.hpp"
#include "log_source.hpp"
#include "tracked_source_manager.hpp"

namespace slayerlog
{

namespace
{

std::filesystem::path make_unique_test_path(const std::string& suffix = ".log")
{
    const auto unique_suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    return std::filesystem::temp_directory_path() / ("slayerlog_tracked_source_manager_" + unique_suffix + suffix);
}

class ScopedTestFile
{
public:
    explicit ScopedTestFile(std::filesystem::path path) : _path(std::move(path))
    {
        std::filesystem::create_directories(_path.parent_path());
        std::ofstream output(_path, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            throw std::runtime_error("Failed to create test file");
        }
    }

    ~ScopedTestFile()
    {
        std::error_code error;
        std::filesystem::remove(_path, error);
    }

    const std::filesystem::path& path() const { return _path; }

    void write(const std::string& content) const
    {
        std::ofstream output(_path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(output.is_open());
        output << content;
        output.close();
        ASSERT_TRUE(output.good());
    }

    void append(const std::string& content) const
    {
        std::ofstream output(_path, std::ios::binary | std::ios::app);
        ASSERT_TRUE(output.is_open());
        output << content;
        output.close();
        ASSERT_TRUE(output.good());
    }

private:
    std::filesystem::path _path;
};

std::vector<std::string> merged_texts(const LogBatch& batch)
{
    std::vector<std::string> texts;
    for (const auto& line : merge_log_batch(batch))
    {
        texts.push_back(line.text);
    }

    return texts;
}

} // namespace

TEST(TrackedSourceManagerTest, OpenSourceLoadsInitialContentsAndPollReturnsOnlyNewLines)
{
    const auto path = make_unique_test_path();
    ScopedTestFile file(path);
    file.write("first\n");

    TrackedSourceManager manager;
    ASSERT_FALSE(manager.open_source(path.string()).has_value());

    EXPECT_EQ(merged_texts(manager.snapshot()), (std::vector<std::string> {"first"}));

    file.append("second\nthird\n");
    EXPECT_EQ(merged_texts(manager.poll()), (std::vector<std::string> {
                                                "second",
                                                "third",
                                            }));
    EXPECT_EQ(merged_texts(manager.snapshot()), (std::vector<std::string> {
                                                    "first",
                                                    "second",
                                                    "third",
                                                }));
}

TEST(TrackedSourceManagerTest, RebuildsSourceLabelsWhenBasenameCollisionsChange)
{
    const auto root        = make_unique_test_path("");
    const auto first_path  = root / "first" / "app.log";
    const auto second_path = root / "second" / "app.log";
    ScopedTestFile first_file(first_path);
    ScopedTestFile second_file(second_path);

    TrackedSourceManager manager;
    ASSERT_FALSE(manager.open_source(first_path.string()).has_value());
    ASSERT_FALSE(manager.open_source(second_path.string()).has_value());

    const auto labels_with_collision = manager.source_labels();
    ASSERT_EQ(labels_with_collision.size(), 2U);
    EXPECT_EQ(labels_with_collision[0], first_path.string());
    EXPECT_EQ(labels_with_collision[1], second_path.string());

    std::string closed_label;
    ASSERT_FALSE(manager.close_source(1, &closed_label).has_value());
    EXPECT_EQ(closed_label, second_path.string());

    const auto labels_without_collision = manager.source_labels();
    ASSERT_EQ(labels_without_collision.size(), 1U);
    EXPECT_EQ(labels_without_collision[0], "app.log");
}

TEST(TrackedSourceManagerTest, OpenFolderLoadsInitialContentsAsSingleTrackedSource)
{
    const auto root   = make_unique_test_path("");
    const auto folder = root / "archive";
    const auto first  = folder / "alpha.log";
    const auto second = folder / "beta.log";
    ScopedTestFile first_file(first);
    ScopedTestFile second_file(second);
    first_file.write("2026-04-01T10:02:00 alpha second\n");
    second_file.write("2026-04-01T10:01:00 beta first\n");

    TrackedSourceManager manager;
    ASSERT_FALSE(manager.open_source(make_local_folder_source(folder.string())).has_value());

    EXPECT_EQ(manager.source_count(), 1U);
    EXPECT_EQ(manager.source_labels(), (std::vector<std::string> {"archive"}));
    EXPECT_EQ(merged_texts(manager.snapshot()), (std::vector<std::string> {
                                                    "2026-04-01T10:01:00 beta first",
                                                    "2026-04-01T10:02:00 alpha second",
                                                }));
    EXPECT_TRUE(manager.poll().empty());
}

} // namespace slayerlog
