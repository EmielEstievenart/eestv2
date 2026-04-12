#pragma once

#include <string>
#include <vector>

#include "log_watcher_base.hpp"

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
    static std::vector<std::string> load_sorted_folder_lines(const std::string& folder_path);

    std::string _folder_path;
    bool _loaded = false;
};

} // namespace slayerlog
