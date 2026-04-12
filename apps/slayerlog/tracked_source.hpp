#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "log_source.hpp"
#include "log_timestamp.hpp"

namespace slayerlog
{

struct LogEntry
{
    std::string raw_text;
    std::optional<LogTimePoint> timestamp;
    std::string extracted_timestamp_text;
    std::uint64_t sequence_number = 0;
};

class TrackedSource
{
public:
    TrackedSource(LogSource source, std::string source_label);

    const LogSource& source() const;
    const std::string& source_label() const;
    void set_source_label(std::string source_label);

    void add_entry_from_raw_string(std::string_view text);
    void add_entries_from_raw_strings(const std::vector<std::string>& lines);

    const std::vector<LogEntry>& entries() const;

private:
    static std::string extract_timestamp_text(std::string_view text, bool has_timestamp);

    LogSource _source;
    std::string _source_label;
    std::vector<LogEntry> _entries;
    std::uint64_t _next_sequence_number = 0;
};

} // namespace slayerlog
