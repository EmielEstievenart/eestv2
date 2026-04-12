#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "tracked_source.hpp"
#include "watchers/log_watcher_base.hpp"

class TrackedSourceManager
{
public:
    void add_pair(std::unique_ptr<slayerlog::LogWatcherBase> watcher, std::unique_ptr<TrackedSource> source);
    void poll_all();

private:
    std::vector<std::pair<std::unique_ptr<slayerlog::LogWatcherBase>, std::unique_ptr<TrackedSource>>> _watcher_source_pairs;
};
