#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "log_batch.hpp"
#include "log_line.hpp"
#include "log_source.hpp"
#include "log_timestamp.hpp"
#include "watchers/log_watcher_base.hpp"

namespace slayerlog
{

using LogEntry = RawLogLine;

class TrackedSource
{
public:
    TrackedSource(LogSource source, std::string source_label, std::shared_ptr<const TimestampFormatCatalog> timestamp_formats = default_timestamp_format_catalog());

    const LogSource& source() const;
    const std::string& source_label() const;
    void set_source_label(std::string source_label);

    void add_entry_from_raw_string(std::string_view text);
    void add_entry(ParsedLogLine line);
    void add_entries_from_raw_strings(std::vector<std::string> lines);
    void add_entries(std::vector<ParsedLogLine> lines);

    bool poll();

    const std::vector<LogEntry>& entries() const;

private:
    struct ChildState
    {
        std::unique_ptr<LogWatcherBase> watcher;
        SourceTimestampParser timestamp_parser;
        bool is_zstd                     = false;
        std::uint64_t next_line_sequence = 0;
    };

    bool poll_single();
    bool poll_folder();
    void refresh_active_children();
    void remove_inactive_children();

    LogSource _source;
    std::string _source_label;
    std::vector<LogEntry> _entries;
    std::uint64_t _next_sequence_number = 0;
    std::shared_ptr<const TimestampFormatCatalog> _timestamp_formats;
    SourceTimestampParser _timestamp_parser;
    std::unique_ptr<LogWatcherBase> _single_watcher;
    std::unordered_map<std::string, ChildState> _children;
    std::unordered_set<std::string> _consumed_zstd_paths;
    std::vector<std::string> _active_file_order;
    std::unordered_set<std::string> _active_file_paths;
};

} // namespace slayerlog
