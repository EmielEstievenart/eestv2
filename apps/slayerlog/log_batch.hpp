#pragma once

#include <memory>
#include <vector>

#include "tracked_sources/log_line.hpp"

namespace slayerlog
{

std::vector<std::shared_ptr<LogEntry>> merge_log_batch(const std::vector<std::shared_ptr<LogEntry>>& batch);
std::vector<LogEntry> merge_log_batch(const std::vector<LogEntry>& batch);

} // namespace slayerlog
