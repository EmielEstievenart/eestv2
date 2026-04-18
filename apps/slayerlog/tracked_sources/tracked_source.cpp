#include "tracked_source.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <utility>

#include "watchers/file_watcher.hpp"
#include "watchers/ssh_tail_watcher.hpp"
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

std::unique_ptr<LogWatcherBase> create_single_watcher_for_source(const LogSource& source)
{
    if (source.kind == LogSourceKind::SshRemoteFile)
    {
        return std::make_unique<SshTailWatcher>(source);
    }

    if (source.kind == LogSourceKind::LocalFile)
    {
        return std::make_unique<FileWatcher>(source.local_path);
    }

    return nullptr;
}

} // namespace

TrackedSource::TrackedSource(LogSource source, std::string source_label, std::shared_ptr<const TimestampFormatCatalog> timestamp_formats)
    : _source(std::move(source)), _source_label(std::move(source_label)), _timestamp_formats(std::move(timestamp_formats)), _timestamp_parser(_timestamp_formats)
{
    if (_timestamp_formats == nullptr)
    {
        _timestamp_formats = default_timestamp_format_catalog();
        _timestamp_parser   = SourceTimestampParser(_timestamp_formats);
    }

    _single_watcher = create_single_watcher_for_source(_source);
}

const LogSource& TrackedSource::source() const
{
    return _source;
}

const std::string& TrackedSource::source_label() const
{
    return _source_label;
}

void TrackedSource::set_source_label(std::string source_label)
{
    _source_label = std::move(source_label);
}

void TrackedSource::add_entry_from_raw_string(std::string_view text)
{
    std::string owned_text(text);
    auto timestamp = _timestamp_parser.parse(owned_text);
    add_entry(ParsedLogLine {std::move(owned_text), std::move(timestamp)});
}

void TrackedSource::add_entry(ParsedLogLine line)
{
    std::optional<LogTimePoint> timestamp;
    std::string extracted_timestamp_text;
    std::string parsed_timestamp_text;
    if (line.timestamp.has_value())
    {
        timestamp                = line.timestamp->time_point;
        extracted_timestamp_text = line.timestamp->extracted_text;
        parsed_timestamp_text    = line.timestamp->display_text;
    }

    _entries.push_back(LogEntry {
        std::move(line.text),
        timestamp,
        std::move(extracted_timestamp_text),
        std::move(parsed_timestamp_text),
        _next_sequence_number++,
    });
}

void TrackedSource::add_entries_from_raw_strings(std::vector<std::string> lines)
{
    _entries.reserve(_entries.size() + lines.size());
    for (auto& line : lines)
    {
        auto timestamp = _timestamp_parser.parse(line);
        add_entry(ParsedLogLine {std::move(line), std::move(timestamp)});
    }
}

void TrackedSource::add_entries(std::vector<ParsedLogLine> lines)
{
    _entries.reserve(_entries.size() + lines.size());
    for (auto& line : lines)
    {
        add_entry(std::move(line));
    }
}

bool TrackedSource::poll()
{
    if (_source.kind == LogSourceKind::LocalFolder)
    {
        return poll_folder();
    }

    return poll_single();
}

const std::vector<LogEntry>& TrackedSource::entries() const
{
    return _entries;
}

bool TrackedSource::poll_single()
{
    if (_single_watcher == nullptr)
    {
        return false;
    }

    std::vector<std::string> lines;
    if (!_single_watcher->poll(lines) || lines.empty())
    {
        return false;
    }

    add_entries_from_raw_strings(std::move(lines));
    return true;
}

bool TrackedSource::poll_folder()
{
    refresh_active_children();

    LogBatch batch;
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
            if (child.is_zstd)
            {
                _consumed_zstd_paths.insert(path_key);
                _children.erase(child_it);
            }
            continue;
        }

        batch.reserve(batch.size() + child_lines.size());
        for (auto& line : child_lines)
        {
            const auto parsed_timestamp = child.timestamp_parser.parse(line);
            LogBatchEntry batch_entry;
            batch_entry.source_index = source_index;
            batch_entry.source_label = _source_label;
            batch_entry.text         = std::move(line);
            if (parsed_timestamp.has_value())
            {
                batch_entry.timestamp           = parsed_timestamp->time_point;
                batch_entry.parsed_time_text    = parsed_timestamp->display_text;
                batch_entry.extracted_time_text = parsed_timestamp->extracted_text;
            }

            batch_entry.source_sequence_number = child.next_line_sequence++;
            batch.push_back(std::move(batch_entry));
        }

        if (child.is_zstd)
        {
            _consumed_zstd_paths.insert(path_key);
            _children.erase(child_it);
        }
    }

    if (batch.empty())
    {
        return false;
    }

    auto merged = merge_log_batch(batch);
    std::vector<ParsedLogLine> lines;
    lines.reserve(merged.size());
    for (auto& line : merged)
    {
        ParsedLogLine parsed_line;
        parsed_line.text = std::move(line.text);
        if (line.timestamp.has_value())
        {
            parsed_line.timestamp = ParsedLogTimestamp {
                *line.timestamp,
                std::move(line.extracted_time_text),
                std::move(line.parsed_time_text),
            };
        }

        lines.push_back(std::move(parsed_line));
    }

    add_entries(std::move(lines));
    return true;
}

void TrackedSource::refresh_active_children()
{
    _active_file_order.clear();
    _active_file_paths.clear();

    for (const auto& file_path : enumerate_regular_files(_source.local_folder_path))
    {
        const std::string path_key = normalize_file_path_for_key(file_path);
        _active_file_order.push_back(path_key);
        _active_file_paths.insert(path_key);

        if (_children.find(path_key) != _children.end())
        {
            continue;
        }

        const bool is_zstd = has_zstd_extension(file_path);
        if (is_zstd && _consumed_zstd_paths.find(path_key) != _consumed_zstd_paths.end())
        {
            continue;
        }

        ChildState child;
        child.is_zstd          = is_zstd;
        child.timestamp_parser = SourceTimestampParser(_timestamp_formats);
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

void TrackedSource::remove_inactive_children()
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
