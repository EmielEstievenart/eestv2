#include "tracked_source.hpp"

#include <algorithm>
#include <cctype>

namespace slayerlog
{

namespace
{

std::size_t skip_leading_whitespace(std::string_view text)
{
    std::size_t index = 0;
    while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])) != 0)
    {
        ++index;
    }

    return index;
}

} // namespace

TrackedSource::TrackedSource(LogSource source, std::string source_label) : _source(std::move(source)), _source_label(std::move(source_label))
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
    const std::string raw_text(text);
    const auto timestamp = parse_log_timestamp(raw_text);

    _entries.push_back(LogEntry {
        raw_text,
        timestamp,
        extract_timestamp_text(text, timestamp.has_value()),
        _next_sequence_number++,
    });
}

void TrackedSource::add_entries_from_raw_strings(const std::vector<std::string>& lines)
{
    _entries.reserve(_entries.size() + lines.size());
    for (const auto& line : lines)
    {
        add_entry_from_raw_string(line);
    }
}

const std::vector<LogEntry>& TrackedSource::entries() const
{
    return _entries;
}

std::string TrackedSource::extract_timestamp_text(std::string_view text, bool has_timestamp)
{
    if (!has_timestamp)
    {
        return {};
    }

    std::size_t start = skip_leading_whitespace(text);
    if (start >= text.size())
    {
        return {};
    }

    if (text[start] == '[')
    {
        const auto closing_bracket = text.find(']', start + 1);
        if (closing_bracket != std::string_view::npos)
        {
            return std::string(text.substr(start, closing_bracket - start + 1));
        }
    }

    std::size_t end = text.find_first_of(" \t", start);
    if (end == std::string_view::npos)
    {
        end = text.size();
    }

    return std::string(text.substr(start, end - start));
}

} // namespace slayerlog
