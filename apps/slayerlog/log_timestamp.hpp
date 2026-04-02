#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace slayerlog
{

using LogTimePoint = std::chrono::system_clock::time_point;

std::optional<LogTimePoint> parse_log_timestamp(const std::string& line);

} // namespace slayerlog
