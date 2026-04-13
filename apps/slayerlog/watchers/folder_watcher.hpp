#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "log_watcher_base.hpp"
#include "log_timestamp.hpp"

namespace slayerlog
{

class FolderWatcher : public LogWatcherBase
{
public:
    explicit FolderWatcher(std::string folder_path);
    explicit FolderWatcher(std::string folder_path, std::shared_ptr<const TimestampFormatCatalog> timestamp_formats);

    std::vector<ParsedLogLine> poll_parsed_lines();

    FolderWatcher(const FolderWatcher&)            = delete;
    FolderWatcher& operator=(const FolderWatcher&) = delete;

protected:
    bool poll_locked(std::vector<std::string>& lines) override;

private:
    struct ChildWatcher
    {
        std::unique_ptr<LogWatcherBase> watcher;
        bool is_zstd                     = false;
        std::uint64_t next_line_sequence = 0;
        SourceTimestampParser timestamp_parser;
    };

    void refresh_active_children();
    void remove_inactive_children();

    std::string _folder_path;
    std::shared_ptr<const TimestampFormatCatalog> _timestamp_formats;
    std::unordered_map<std::string, ChildWatcher> _children;
    std::unordered_set<std::string> _consumed_zstd_paths;
    std::vector<std::string> _active_file_order;
    std::unordered_set<std::string> _active_file_paths;
};

} // namespace slayerlog
