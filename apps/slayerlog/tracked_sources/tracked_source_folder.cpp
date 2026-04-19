#include "tracked_source_folder.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <utility>

#include "log_batch.hpp"
#include "watchers/file_watcher.hpp"
#include "watchers/zstd_file_watcher.hpp"

namespace slayerlog
{

namespace
{

std::string normalize_file_path_for_key(const std::filesystem::path& path)
{
    std::error_code error_code;
    std::filesystem::path normalized_path = std::filesystem::weakly_canonical(path, error_code);
    if (error_code)
    {
        error_code.clear();
        normalized_path = std::filesystem::absolute(path, error_code);
        if (error_code)
        {
            normalized_path = path.lexically_normal();
        }
        else
        {
            normalized_path = normalized_path.lexically_normal();
        }
    }

    std::string normalized_text = normalized_path.make_preferred().string();
#ifdef _WIN32
    std::transform(normalized_text.begin(), normalized_text.end(), normalized_text.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
#endif
    return normalized_text;
}

bool has_zstd_extension(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension == ".zst";
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

} // namespace

TrackedSourceFolder::TrackedSourceFolder(LogSource source, std::string source_label, std::shared_ptr<const TimestampFormatCatalog> timestamp_formats)
    : TrackedSourceBase(std::move(source), std::move(source_label), std::move(timestamp_formats))
{
}

bool TrackedSourceFolder::poll()
{
    refresh_active_children();

    std::vector<LogEntry> batch;
    for (std::size_t source_index = 0; source_index < _active_file_order.size(); ++source_index)
    {
        const std::string& path_key = _active_file_order[source_index];
        auto child_it               = _children.find(path_key);
        if (child_it == _children.end())
        {
            continue;
        }

        auto& child = child_it->second;
        std::vector<std::string> child_lines;
        if (!child.watcher->poll(child_lines) || child_lines.empty())
        {
            continue;
        }

        if (!child.timestamp_parser_initialized)
        {
            for (const auto& line : child_lines)
            {
                LogEntry probe(line);
                const auto catalog = timestamp_formats();
                if (catalog == nullptr || !child.timestamp_parser.init(probe, *catalog))
                {
                    continue;
                }

                child.timestamp_parser_initialized = true;
                break;
            }
        }

        batch.reserve(batch.size() + child_lines.size());
        for (auto& line : child_lines)
        {
            LogEntry batch_entry;
            batch_entry.metadata.source_index = source_index;
            batch_entry.metadata.source_label = source_label();
            batch_entry.text                  = std::move(line);
            child.timestamp_parser.parse(batch_entry);
            batch.push_back(std::move(batch_entry));
        }
    }

    if (batch.empty())
    {
        return false;
    }

    auto merged = merge_log_batch(batch);
    reserve_entries(merged.size());
    for (auto& line : merged)
    {
        LogEntry& entry                    = append_entry();
        entry.text                         = std::move(line.text);
        entry.metadata.timestamp           = std::move(line.metadata.timestamp);
        entry.metadata.extracted_time_text = std::move(line.metadata.extracted_time_text);
        entry.metadata.parsed_time_text    = std::move(line.metadata.parsed_time_text);
    }

    return true;
}

void TrackedSourceFolder::refresh_active_children()
{
    _active_file_order.clear();
    _active_file_paths.clear();

    for (const auto& file_path : enumerate_regular_files(source().local_folder_path))
    {
        const std::string path_key = normalize_file_path_for_key(file_path);
        _active_file_order.push_back(path_key);
        _active_file_paths.insert(path_key);

        if (_children.find(path_key) != _children.end())
        {
            continue;
        }

        const bool is_zstd = has_zstd_extension(file_path);

        ChildState child;
        if (is_zstd)
        {
            child.watcher = std::make_unique<ZstdFileWatcher>(file_path.string());
        }
        else
        {
            child.watcher = std::make_unique<FileWatcher>(file_path.string());
        }

        _children.emplace(path_key, std::move(child));
    }

    remove_inactive_children();
}

void TrackedSourceFolder::remove_inactive_children()
{
    for (auto child_it = _children.begin(); child_it != _children.end();)
    {
        if (_active_file_paths.find(child_it->first) != _active_file_paths.end())
        {
            ++child_it;
            continue;
        }

        child_it = _children.erase(child_it);
    }
}

} // namespace slayerlog
