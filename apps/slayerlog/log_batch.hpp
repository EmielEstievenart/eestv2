#pragma once

#include <vector>

#include "tracked_sources/log_line.hpp"

namespace slayerlog
{

std::vector<LogEntry> merge_log_batch(const std::vector<LogEntry>& batch);

} // namespace slayerlog
