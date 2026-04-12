#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "watchers/folder_watcher.hpp"

namespace slayerlog
{

namespace
{

std::filesystem::path make_unique_test_folder()
{
    const auto unique_suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    return std::filesystem::temp_directory_path() / ("slayerlog_folder_watcher_" + unique_suffix);
}

class ScopedTestFolder
{
public:
    ScopedTestFolder()
    {
        _path = make_unique_test_folder();
        std::filesystem::create_directories(_path);
    }

    ~ScopedTestFolder()
    {
        std::error_code error;
        std::filesystem::remove_all(_path, error);
    }

    const std::filesystem::path& path() const { return _path; }

    void write_file(const std::string& file_name, const std::string& content) const
    {
        const auto file_path = _path / file_name;
        std::ofstream output(file_path, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            throw std::runtime_error("Failed to create test file");
        }

        output << content;
    }

    void write_zstd_file(const std::string& file_name, const std::vector<unsigned char>& bytes) const
    {
        const auto file_path = _path / file_name;
        std::ofstream output(file_path, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            throw std::runtime_error("Failed to create test zstd file");
        }

        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

private:
    std::filesystem::path _path;
};

void expect_poll_lines(FolderWatcher& watcher, const std::vector<std::string>& expected_lines)
{
    std::vector<std::string> lines {"stale data"};
    ASSERT_TRUE(watcher.poll(lines));
    EXPECT_EQ(lines, expected_lines);
}

void expect_no_poll_lines(FolderWatcher& watcher)
{
    std::vector<std::string> lines {"stale data"};
    EXPECT_FALSE(watcher.poll(lines));
    EXPECT_TRUE(lines.empty());
}

} // namespace

TEST(FolderWatcherTest, FirstPollReturnsFilesMergedByTimestamp)
{
    ScopedTestFolder folder;
    folder.write_file("alpha.log", "2026-04-01T10:02:00 alpha second\nplain alpha follow-up\n");
    folder.write_file("beta.log", "2026-04-01T10:01:00 beta first\n");

    FolderWatcher watcher(folder.path().string());
    expect_poll_lines(watcher, {
                                   "2026-04-01T10:01:00 beta first",
                                   "2026-04-01T10:02:00 alpha second",
                                   "plain alpha follow-up",
                               });
    expect_no_poll_lines(watcher);
}

TEST(FolderWatcherTest, ReadsZstdCompressedFiles)
{
    ScopedTestFolder folder;
    folder.write_file("alpha.log", "2026-04-01T10:02:00 alpha second\n");
    folder.write_zstd_file("beta.log.zst", {
                                               0x28, 0xb5, 0x2f, 0xfd, 0x20, 0x31, 0x89, 0x01, 0x00, 0x32, 0x30, 0x32, 0x36, 0x2d, 0x30, 0x34, 0x2d, 0x30, 0x31, 0x54, 0x31, 0x30, 0x3a, 0x30, 0x31, 0x3a, 0x30, 0x30, 0x20,
                                               0x66, 0x72, 0x6f, 0x6d, 0x20, 0x7a, 0x73, 0x74, 0x0a, 0x70, 0x6c, 0x61, 0x69, 0x6e, 0x20, 0x7a, 0x73, 0x74, 0x20, 0x66, 0x6f, 0x6c, 0x6c, 0x6f, 0x77, 0x2d, 0x75, 0x70, 0x0a,
                                           });

    FolderWatcher watcher(folder.path().string());
    expect_poll_lines(watcher, {
                                   "2026-04-01T10:01:00 from zst",
                                   "plain zst follow-up",
                                   "2026-04-01T10:02:00 alpha second",
                               });
}

TEST(FolderWatcherTest, MissingFolderThrowsWhenPolled)
{
    const auto missing_path = make_unique_test_folder();
    FolderWatcher watcher(missing_path.string());

    std::vector<std::string> lines;
    EXPECT_THROW(watcher.poll(lines), std::runtime_error);
}

} // namespace slayerlog
