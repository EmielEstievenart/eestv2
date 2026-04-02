#pragma once

#include <string>
#include <vector>

namespace slayerlog
{

struct ObservedLogLine
{
    std::string source_label;
    std::string text;
};

using WatcherLineBatch = std::vector<std::string>;

std::vector<ObservedLogLine> merge_log_batch(
    const std::vector<WatcherLineBatch>& watcher_batches,
    const std::vector<std::string>& source_labels);

} // namespace slayerlog
