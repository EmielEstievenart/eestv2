#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace slayerlog
{

using LogTimePoint = std::chrono::system_clock::time_point;

struct ParsedLogTimestamp
{
    LogTimePoint time_point;
    std::string extracted_text;
    std::string display_text;
};

struct ParsedLogLine
{
    std::string text;
    std::optional<ParsedLogTimestamp> timestamp;
};

class TimestampFormatCatalog
{
public:
    struct Entry
    {
        std::string format;
        std::shared_ptr<void> compiled_parser;
    };

    explicit TimestampFormatCatalog(std::vector<std::string> formats);
    ~TimestampFormatCatalog();

    TimestampFormatCatalog(const TimestampFormatCatalog&)                = default;
    TimestampFormatCatalog& operator=(const TimestampFormatCatalog&)     = default;
    TimestampFormatCatalog(TimestampFormatCatalog&&) noexcept            = default;
    TimestampFormatCatalog& operator=(TimestampFormatCatalog&&) noexcept = default;

    const std::vector<std::string>& formats() const;
    const std::vector<Entry>& entries() const;

private:
    std::vector<std::string> _formats;
    std::vector<Entry> _entries;
};

std::vector<std::string> default_timestamp_formats();
std::shared_ptr<const TimestampFormatCatalog> default_timestamp_format_catalog();
void set_default_timestamp_format_catalog(std::shared_ptr<const TimestampFormatCatalog> catalog);

class SourceTimestampParser
{
public:
    explicit SourceTimestampParser(std::shared_ptr<const TimestampFormatCatalog> catalog = default_timestamp_format_catalog());

    std::optional<ParsedLogTimestamp> parse(const std::string& line);

private:
    std::shared_ptr<const TimestampFormatCatalog> _catalog;
    std::optional<std::size_t> _detected_format_index;
    std::optional<std::size_t> _detected_start_index_slot;
};

std::optional<ParsedLogTimestamp> parse_log_timestamp_details(const std::string& line);
std::optional<LogTimePoint> parse_log_timestamp(const std::string& line);

} // namespace slayerlog
