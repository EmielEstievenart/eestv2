#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace slayerlog
{

class TrackedSourceBase;

struct LogEntryMetadata
{
    std::optional<std::chrono::system_clock::time_point> timestamp;
    std::chrono::system_clock::duration time_offset {};
    std::string extracted_time_text;
    std::string parsed_time_text;
    std::optional<std::size_t> extracted_time_start;
    std::optional<std::size_t> extracted_time_end;
    std::uint64_t sequence_number = 0;
    std::size_t source_index = 0;
    std::string source_label;
    TrackedSourceBase* source = nullptr;

    LogEntryMetadata() = default;

    LogEntryMetadata(std::optional<std::chrono::system_clock::time_point> timestamp, std::chrono::system_clock::duration time_offset = {}, std::string extracted_time_text = {},
                     std::string parsed_time_text = {})
        : timestamp(std::move(timestamp)), time_offset(time_offset), extracted_time_text(std::move(extracted_time_text)), parsed_time_text(std::move(parsed_time_text))
    {
    }
};

inline std::optional<std::chrono::system_clock::time_point> effective_timestamp(const LogEntryMetadata& metadata)
{
    if (!metadata.timestamp.has_value())
    {
        return std::nullopt;
    }

    return metadata.timestamp.value() + metadata.time_offset;
}

struct LogEntry
{
    std::string text;
    LogEntryMetadata metadata;

    LogEntry() = default;

    LogEntry(std::string text, std::optional<std::chrono::system_clock::time_point> timestamp = std::nullopt, std::chrono::system_clock::duration time_offset = {}, std::string extracted_time_text = {},
             std::string parsed_time_text = {})
        : text(std::move(text)), metadata(std::move(timestamp), time_offset, std::move(extracted_time_text), std::move(parsed_time_text))
    {
    }

    LogEntry(std::string source_label, std::string text, std::optional<std::chrono::system_clock::time_point> timestamp = std::nullopt, std::chrono::system_clock::duration time_offset = {},
             std::string parsed_time_text = {}, std::string extracted_time_text = {})
        : LogEntry(std::move(text), std::move(timestamp), time_offset, std::move(extracted_time_text), std::move(parsed_time_text))
    {
        metadata.source_label = std::move(source_label);
    }

    LogEntry(std::size_t source_index, std::string source_label, std::string text, std::optional<std::chrono::system_clock::time_point> timestamp = std::nullopt, std::uint64_t sequence_number = 0,
             std::chrono::system_clock::duration time_offset = {}, std::string parsed_time_text = {}, std::string extracted_time_text = {})
        : LogEntry(std::move(source_label), std::move(text), std::move(timestamp), time_offset, std::move(parsed_time_text), std::move(extracted_time_text))
    {
        metadata.source_index    = source_index;
        metadata.sequence_number = sequence_number;
    }
};

} // namespace slayerlog
