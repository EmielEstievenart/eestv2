#include "folder_watcher.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <zstd.h>

#include "debug_log.hpp"
#include "log_batch.hpp"
#include "log_timestamp.hpp"

namespace slayerlog
{

namespace
{

std::string to_lower_copy(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return text;
}

bool has_zstd_extension(const std::filesystem::path& path)
{
    return to_lower_copy(path.extension().string()) == ".zst";
}

std::vector<std::filesystem::path> enumerate_regular_files(const std::string& folder_path)
{
    const std::filesystem::path path(folder_path);
    if (!std::filesystem::exists(path))
    {
        throw std::runtime_error("Folder does not exist: " + folder_path);
    }

    if (!std::filesystem::is_directory(path))
    {
        throw std::runtime_error("Path is not a folder: " + folder_path);
    }

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(path))
    {
        if (entry.is_regular_file())
        {
            files.push_back(entry.path());
        }
    }

    std::sort(files.begin(), files.end(), [](const std::filesystem::path& lhs, const std::filesystem::path& rhs) { return lhs.lexically_normal().generic_string() < rhs.lexically_normal().generic_string(); });
    return files;
}

std::string read_binary_file(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open())
    {
        throw std::runtime_error("Failed to open file: " + path.string());
    }

    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::string decompress_zstd_bytes(std::string_view compressed_bytes, const std::filesystem::path& path)
{
    struct DStreamDeleter
    {
        void operator()(ZSTD_DStream* stream) const { ZSTD_freeDStream(stream); }
    };

    std::unique_ptr<ZSTD_DStream, DStreamDeleter> stream(ZSTD_createDStream());
    if (stream == nullptr)
    {
        throw std::runtime_error("Failed to create zstd stream for file: " + path.string());
    }

    std::size_t result = ZSTD_initDStream(stream.get());
    if (ZSTD_isError(result) != 0)
    {
        throw std::runtime_error("Failed to initialize zstd stream for file: " + path.string());
    }

    ZSTD_inBuffer input {compressed_bytes.data(), compressed_bytes.size(), 0};
    std::array<char, 1 << 15> buffer {};
    std::string output;

    while (input.pos < input.size)
    {
        ZSTD_outBuffer out {buffer.data(), buffer.size(), 0};
        result = ZSTD_decompressStream(stream.get(), &out, &input);
        if (ZSTD_isError(result) != 0)
        {
            throw std::runtime_error("Failed to decompress zstd file: " + path.string());
        }

        output.append(buffer.data(), out.pos);

        if (result == 0 && input.pos < input.size)
        {
            result = ZSTD_initDStream(stream.get());
            if (ZSTD_isError(result) != 0)
            {
                throw std::runtime_error("Failed to continue zstd decompression for file: " + path.string());
            }
        }
    }

    if (result != 0)
    {
        throw std::runtime_error("Incomplete zstd data in file: " + path.string());
    }

    return output;
}

std::string read_log_file_contents(const std::filesystem::path& path)
{
    const std::string bytes = read_binary_file(path);
    if (has_zstd_extension(path))
    {
        return decompress_zstd_bytes(bytes, path);
    }

    return bytes;
}

std::vector<std::string> split_lines(std::string text)
{
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < text.size())
    {
        const auto newline = text.find('\n', start);
        if (newline == std::string::npos)
        {
            std::string line = text.substr(start);
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            // Folder loads are snapshot-only for now, so keep a final unterminated line.
            lines.push_back(std::move(line));
            break;
        }

        std::string line = text.substr(start, newline - start);
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        lines.push_back(std::move(line));
        start = newline + 1;
    }

    return lines;
}

std::vector<std::string> merge_file_lines(const std::vector<std::filesystem::path>& files)
{
    LogBatch batch;

    for (std::size_t file_index = 0; file_index < files.size(); ++file_index)
    {
        const auto lines = split_lines(read_log_file_contents(files[file_index]));
        batch.reserve(batch.size() + lines.size());
        for (std::size_t line_index = 0; line_index < lines.size(); ++line_index)
        {
            const std::string& line = lines[line_index];
            batch.push_back(LogBatchEntry {
                file_index,
                {},
                line,
                parse_log_timestamp(line),
                static_cast<std::uint64_t>(line_index),
            });
        }
    }

    const auto merged = merge_log_batch(batch);
    std::vector<std::string> lines;
    lines.reserve(merged.size());
    for (const auto& line : merged)
    {
        lines.push_back(line.text);
    }

    return lines;
}

} // namespace

FolderWatcher::FolderWatcher(std::string folder_path) : _folder_path(std::move(folder_path))
{
    SLAYERLOG_LOG_INFO("Created folder watcher for folder=" << _folder_path);
}

bool FolderWatcher::poll_locked(std::vector<std::string>& lines)
{
    if (_loaded)
    {
        return false;
    }

    lines   = load_sorted_folder_lines(_folder_path);
    _loaded = true;
    return !lines.empty();
}

std::vector<std::string> FolderWatcher::load_sorted_folder_lines(const std::string& folder_path)
{
    return merge_file_lines(enumerate_regular_files(folder_path));
}

} // namespace slayerlog
