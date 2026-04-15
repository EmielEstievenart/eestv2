#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <zstd.h>

#include "all_tracked_sources.hpp"
#include "log_source.hpp"

namespace slayerlog
{

namespace
{

std::filesystem::path make_unique_test_path(const std::string& suffix = ".log")
{
    const auto unique_suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    return std::filesystem::temp_directory_path() / ("slayerlog_tracked_source_manager_" + unique_suffix + suffix);
}

std::vector<unsigned char> compress_zstd_text(std::string_view text)
{
    const std::size_t bound = ZSTD_compressBound(text.size());
    std::vector<unsigned char> compressed(bound);
    const std::size_t written = ZSTD_compress(compressed.data(), compressed.size(), text.data(), text.size(), 1);
    if (ZSTD_isError(written) != 0)
    {
        throw std::runtime_error("Failed to compress zstd test payload");
    }

    compressed.resize(written);
    return compressed;
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

    void write_bytes(const std::vector<unsigned char>& bytes) const
    {
        std::ofstream output(_path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(output.is_open());
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        output.close();
        ASSERT_TRUE(output.good());
    }

private:
    std::filesystem::path _path;
};

std::vector<std::string> all_texts(const AllTrackedSources& tracked_sources)
{
    std::vector<std::string> texts;
    texts.reserve(tracked_sources.all_lines().size());
    for (const auto& line : tracked_sources.all_lines())
    {
        texts.push_back(line.text);
    }

    return texts;
}

std::vector<std::string> delta_texts(const AllTrackedSources& tracked_sources, AllLineIndex first_new_line_index)
{
    std::vector<std::string> texts;
    for (int index = first_new_line_index.value; index < tracked_sources.line_count(); ++index)
    {
        texts.push_back(tracked_sources.all_lines()[AllLineIndex {index}].text);
    }

    return texts;
}

} // namespace

TEST(AllTrackedSourcesTest, OpenSourceLoadsInitialContentsAndPollReturnsOnlyNewLines)
{
    const auto path = make_unique_test_path();
    ScopedTestFile file(path);
    file.write("first\n");

    AllTrackedSources tracked_sources;
    ASSERT_FALSE(tracked_sources.open_source(path.string()).has_value());

    EXPECT_EQ(all_texts(tracked_sources), (std::vector<std::string> {"first"}));

    file.append("second\nthird\n");
    const auto first_new_line_index = tracked_sources.poll();
    ASSERT_TRUE(first_new_line_index.has_value());
    EXPECT_EQ(delta_texts(tracked_sources, *first_new_line_index), (std::vector<std::string> {
                                                                       "second",
                                                                       "third",
                                                                   }));
    EXPECT_EQ(all_texts(tracked_sources), (std::vector<std::string> {
                                              "first",
                                              "second",
                                              "third",
                                          }));
}

TEST(AllTrackedSourcesTest, RebuildsSourceLabelsWhenBasenameCollisionsChange)
{
    const auto root        = make_unique_test_path("");
    const auto first_path  = root / "first" / "app.log";
    const auto second_path = root / "second" / "app.log";
    ScopedTestFile first_file(first_path);
    ScopedTestFile second_file(second_path);

    AllTrackedSources tracked_sources;
    ASSERT_FALSE(tracked_sources.open_source(first_path.string()).has_value());
    ASSERT_FALSE(tracked_sources.open_source(second_path.string()).has_value());

    const auto labels_with_collision = tracked_sources.source_labels();
    ASSERT_EQ(labels_with_collision.size(), 2U);
    EXPECT_EQ(labels_with_collision[0], first_path.string());
    EXPECT_EQ(labels_with_collision[1], second_path.string());

    std::string closed_label;
    ASSERT_FALSE(tracked_sources.close_source(1, &closed_label).has_value());
    EXPECT_EQ(closed_label, second_path.string());

    const auto labels_without_collision = tracked_sources.source_labels();
    ASSERT_EQ(labels_without_collision.size(), 1U);
    EXPECT_EQ(labels_without_collision[0], "app.log");
}

TEST(AllTrackedSourcesTest, OpenFolderLoadsInitialContentsAsSingleTrackedSource)
{
    const auto root   = make_unique_test_path("");
    const auto folder = root / "archive";
    const auto first  = folder / "alpha.log";
    const auto second = folder / "beta.log";
    ScopedTestFile first_file(first);
    ScopedTestFile second_file(second);
    first_file.write("2026-04-01T10:02:00 alpha second\n");
    second_file.write("2026-04-01T10:01:00 beta first\n");

    AllTrackedSources tracked_sources;
    ASSERT_FALSE(tracked_sources.open_source(make_local_folder_source(folder.string())).has_value());

    EXPECT_EQ(tracked_sources.source_count(), 1U);
    EXPECT_EQ(tracked_sources.source_labels(), (std::vector<std::string> {"archive"}));
    EXPECT_EQ(all_texts(tracked_sources), (std::vector<std::string> {
                                              "2026-04-01T10:01:00 beta first",
                                              "2026-04-01T10:02:00 alpha second",
                                          }));
    EXPECT_FALSE(tracked_sources.poll().has_value());
}

TEST(AllTrackedSourcesTest, FolderSourceContinuesProducingIncrementalUpdatesAfterOpen)
{
    const auto root       = make_unique_test_path("");
    const auto folder     = root / "archive";
    const auto plain_file = folder / "alpha.log";
    const auto zstd_file  = folder / "beta.log.zst";
    ScopedTestFile first_file(plain_file);
    ScopedTestFile compressed_file(zstd_file);
    first_file.write("2026-04-01T10:02:00 alpha second\n");
    compressed_file.write_bytes(compress_zstd_text("2026-04-01T10:01:00 beta first\n"));

    AllTrackedSources tracked_sources;
    ASSERT_FALSE(tracked_sources.open_source(make_local_folder_source(folder.string())).has_value());

    EXPECT_EQ(all_texts(tracked_sources), (std::vector<std::string> {
                                              "2026-04-01T10:01:00 beta first",
                                              "2026-04-01T10:02:00 alpha second",
                                          }));

    first_file.append("2026-04-01T10:03:00 alpha third\n");
    const auto first_new_line_index = tracked_sources.poll();
    ASSERT_TRUE(first_new_line_index.has_value());
    EXPECT_EQ(delta_texts(tracked_sources, *first_new_line_index), (std::vector<std::string> {
                                                                       "2026-04-01T10:03:00 alpha third",
                                                                   }));

    EXPECT_FALSE(tracked_sources.poll().has_value());
}

} // namespace slayerlog
