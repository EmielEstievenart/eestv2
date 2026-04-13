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

    FolderWatcher(const FolderWatcher&)            = delete;
    FolderWatcher& operator=(const FolderWatcher&) = delete;

protected:
    bool poll_locked(std::vector<std::string>& lines) override;

private:
    using TimestampParser = std::optional<LogTimePoint> (*)(const std::string& line);

    struct ChildWatcher
    {
        std::unique_ptr<LogWatcherBase> watcher;
        bool is_zstd                     = false;
        std::uint64_t next_line_sequence = 0;
    };

    void refresh_active_children();
    void remove_inactive_children();

    std::string _folder_path;
    TimestampParser _timestamp_parser;
    std::unordered_map<std::string, ChildWatcher> _children;
    std::unordered_set<std::string> _consumed_zstd_paths;
    std::vector<std::string> _active_file_order;
    std::unordered_set<std::string> _active_file_paths;
};

} // namespace slayerlog
