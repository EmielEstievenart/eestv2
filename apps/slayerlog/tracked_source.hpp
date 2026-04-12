#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <optional>

struct LogEntry
{
    using timestamp_t = std::chrono::time_point<std::chrono::system_clock, std::chrono::microseconds>;
    std::string raw_text;
    std::optional<timestamp_t> timestamp;

    std::string extracted_timestamp_text;
    std::uint64_t sequence_number {0};
};

class TrackedSource
{
    TrackedSource(std::string source_name) : _source_name {source_name} { };

    void add_entry_from_raw_string(const std::string& string) { };
    void add_entry_from_raw_string(const std::vector<std::string>& strings) { };

private:
    std::vector<LogEntry> _entries;
    const std::string _source_name;
};