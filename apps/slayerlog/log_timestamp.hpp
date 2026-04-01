#pragma once

#include <optional>
#include <string>

#include "log_view_model.hpp"

namespace slayerlog
{

std::optional<LogTimePoint> parse_log_timestamp(const std::string& line);

} // namespace slayerlog
