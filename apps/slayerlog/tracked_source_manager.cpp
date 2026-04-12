#include "tracked_source_manager.hpp"

#include <string>

void TrackedSourceManager::add_pair(std::unique_ptr<slayerlog::LogWatcherBase> watcher, std::unique_ptr<TrackedSource> source)
{
    _watcher_source_pairs.emplace_back(std::move(watcher), std::move(source));
}

void TrackedSourceManager::poll_all()
{
    std::vector<std::string> lines;

    for (auto& [watcher, source] : _watcher_source_pairs)
    {
        if (watcher == nullptr || source == nullptr)
        {
            continue;
        }

        watcher->poll(lines);
    }
}
