#include "tracked_source.hpp"

#include <utility>

namespace slayerlog
{

TrackedSource::TrackedSource(LogSource source, std::string source_label, std::shared_ptr<const TimestampFormatCatalog> timestamp_formats)
    : _source(std::move(source)), _source_label(std::move(source_label)), _timestamp_parser(std::move(timestamp_formats))
{
}

const LogSource& TrackedSource::source() const
{
    return _source;
}

const std::string& TrackedSource::source_label() const
{
    return _source_label;
}

void TrackedSource::set_source_label(std::string source_label)
{
    _source_label = std::move(source_label);
}

void TrackedSource::add_entry_from_raw_string(std::string_view text)
{
    std::string owned_text(text);
    auto timestamp = _timestamp_parser.parse(owned_text);
    add_entry(ParsedLogLine {std::move(owned_text), std::move(timestamp)});
}

void TrackedSource::add_entry(ParsedLogLine line)
{
    std::optional<LogTimePoint> timestamp;
    std::string extracted_timestamp_text;
    std::string parsed_timestamp_text;
    if (line.timestamp.has_value())
    {
        timestamp                = line.timestamp->time_point;
        extracted_timestamp_text = line.timestamp->extracted_text;
        parsed_timestamp_text    = line.timestamp->display_text;
    }

    _entries.push_back(LogEntry {
        std::move(line.text),
        timestamp,
        std::move(extracted_timestamp_text),
        std::move(parsed_timestamp_text),
        _next_sequence_number++,
    });
}

void TrackedSource::add_entries_from_raw_strings(std::vector<std::string> lines)
{
    _entries.reserve(_entries.size() + lines.size());
    for (auto& line : lines)
    {
        auto timestamp = _timestamp_parser.parse(line);
        add_entry(ParsedLogLine {std::move(line), std::move(timestamp)});
    }
}

void TrackedSource::add_entries(std::vector<ParsedLogLine> lines)
{
    _entries.reserve(_entries.size() + lines.size());
    for (auto& line : lines)
    {
        add_entry(std::move(line));
    }
}

const std::vector<LogEntry>& TrackedSource::entries() const
{
    return _entries;
}

} // namespace slayerlog
