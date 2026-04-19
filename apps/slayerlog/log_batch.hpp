#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "log_line.hpp"

namespace slayerlog
{

struct ObservedLogLine : public RawLogLine
{
    std::string source_label;

    ObservedLogLine() = default;

    ObservedLogLine(std::string source_label, std::string text, std::optional<LogTimePoint> timestamp = std::nullopt, std::string parsed_time_text = {}, std::string extracted_time_text = {})
        : RawLogLine(std::move(text), std::move(timestamp), std::move(extracted_time_text), std::move(parsed_time_text)), source_label(std::move(source_label))
    {
    }
};

struct LogBatchEntry : public RawLogLine
{
    std::size_t source_index = 0;
    std::string source_label;

    LogBatchEntry() = default;

    LogBatchEntry(std::size_t source_index, std::string source_label, std::string text, std::optional<LogTimePoint> timestamp = std::nullopt, std::uint64_t sequence_number = 0,
                  std::string parsed_time_text = {}, std::string extracted_time_text = {})
        : RawLogLine(std::move(text), std::move(timestamp), std::move(extracted_time_text), std::move(parsed_time_text)),
          source_index(source_index),
          source_label(std::move(source_label))
    {
        metadata.sequence_number = sequence_number;
    }
};

using LogBatch = std::vector<LogBatchEntry>;

std::vector<ObservedLogLine> merge_log_batch(const LogBatch& batch);

} // namespace slayerlog
