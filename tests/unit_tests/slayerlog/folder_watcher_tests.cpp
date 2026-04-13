#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <zstd.h>

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

    void append_file(const std::string& file_name, const std::string& content) const
    {
        const auto file_path = _path / file_name;
        std::ofstream output(file_path, std::ios::binary | std::ios::app);
        if (!output.is_open())
        {
            throw std::runtime_error("Failed to append test file");
        }

        output << content;
    }

    void write_zstd_file(const std::string& file_name, std::string_view content) const
    {
        const auto bytes     = compress_zstd_text(content);
        const auto file_path = _path / file_name;
        std::ofstream output(file_path, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            throw std::runtime_error("Failed to create test zstd file");
        }

        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    void remove_file(const std::string& file_name) const
    {
        std::error_code error;
        std::filesystem::remove(_path / file_name, error);
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

TEST(FolderWatcherTest, KeepsTailingNormalFilesAfterFirstPoll)
{
    ScopedTestFolder folder;
    folder.write_file("alpha.log", "first\nsecond\n");

    FolderWatcher watcher(folder.path().string());
    expect_poll_lines(watcher, {"first", "second"});

    folder.append_file("alpha.log", "third\nfourth\n");
    expect_poll_lines(watcher, {"third", "fourth"});
    expect_no_poll_lines(watcher);
}

TEST(FolderWatcherTest, DiscoversNewlyCreatedNormalFiles)
{
    ScopedTestFolder folder;
    folder.write_file("alpha.log", "alpha first\n");

    FolderWatcher watcher(folder.path().string());
    expect_poll_lines(watcher, {"alpha first"});

    folder.write_file("beta.log", "beta first\n");
    expect_poll_lines(watcher, {"beta first"});

    folder.append_file("alpha.log", "alpha second\n");
    expect_poll_lines(watcher, {"alpha second"});
}

TEST(FolderWatcherTest, DiscoversNewlyCreatedZstdFilesOnce)
{
    ScopedTestFolder folder;
    folder.write_file("alpha.log", "2026-04-01T10:02:00 alpha second\n");

    FolderWatcher watcher(folder.path().string());
    expect_poll_lines(watcher, {"2026-04-01T10:02:00 alpha second"});

    folder.write_zstd_file("beta.log.zst", "2026-04-01T10:01:00 from zst\nplain zst follow-up\n");
    expect_poll_lines(watcher, {
                                   "2026-04-01T10:01:00 from zst",
                                   "plain zst follow-up",
                               });
    expect_no_poll_lines(watcher);
}

TEST(FolderWatcherTest, DeletedAndRecreatedZstdFileIsNotReread)
{
    ScopedTestFolder folder;
    folder.write_zstd_file("archive.log.zst", "first\n");

    FolderWatcher watcher(folder.path().string());
    expect_poll_lines(watcher, {"first"});

    folder.remove_file("archive.log.zst");
    expect_no_poll_lines(watcher);

    folder.write_zstd_file("archive.log.zst", "second\n");
    expect_no_poll_lines(watcher);
}

TEST(FolderWatcherTest, MergesPlainAndZstdChildResultsByTimestamp)
{
    ScopedTestFolder folder;
    folder.write_file("alpha.log", "2026-04-01T10:02:00 alpha second\n");
    folder.write_zstd_file("beta.log.zst", "2026-04-01T10:01:00 beta first\n");

    FolderWatcher watcher(folder.path().string());
    expect_poll_lines(watcher, {
                                   "2026-04-01T10:01:00 beta first",
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
