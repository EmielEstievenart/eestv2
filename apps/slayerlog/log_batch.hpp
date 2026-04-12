#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "log_timestamp.hpp"

namespace slayerlog
{

struct ObservedLogLine
{
    std::string source_label;
    std::string text;
};

struct LogBatchEntry
{
    std::size_t source_index = 0;
    std::string source_label;
    std::string text;
    std::optional<LogTimePoint> timestamp;
    std::uint64_t source_sequence_number = 0;
};

using LogBatch = std::vector<LogBatchEntry>;

std::vector<ObservedLogLine> merge_log_batch(const LogBatch& batch);

} // namespace slayerlog
