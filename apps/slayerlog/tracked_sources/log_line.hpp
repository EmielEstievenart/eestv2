#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "timestamp/log_time_point.hpp"

namespace slayerlog
{

class TrackedSource;

struct LogLineMetadata
{
    std::optional<LogTimePoint> timestamp;
    std::string extracted_time_text;
    std::string parsed_time_text;
    std::uint64_t sequence_number = 0;
    TrackedSource* source        = nullptr;

    LogLineMetadata() = default;

    LogLineMetadata(std::optional<LogTimePoint> timestamp, std::string extracted_time_text = {}, std::string parsed_time_text = {})
        : timestamp(std::move(timestamp)), extracted_time_text(std::move(extracted_time_text)), parsed_time_text(std::move(parsed_time_text))
    {
    }
};

struct RawLogLine
{
    std::string text;
    LogLineMetadata metadata;

    RawLogLine() = default;

    RawLogLine(std::string text, std::optional<LogTimePoint> timestamp = std::nullopt, std::string extracted_time_text = {}, std::string parsed_time_text = {})
        : text(std::move(text)), metadata(std::move(timestamp), std::move(extracted_time_text), std::move(parsed_time_text))
    {
    }
};

} // namespace slayerlog
