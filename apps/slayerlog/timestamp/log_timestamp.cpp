#include "timestamp/log_timestamp.hpp"

#include <algorithm>
#include <array>
#include <ctime>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "../../code/eestv/timestamp/timestamp_parser.hpp"

namespace slayerlog
{

namespace
{

using eestv::DateAndTime;
using eestv::TimestampParser;

std::time_t make_utc_time(std::tm* utc_time)
{
#ifdef _WIN32
    return _mkgmtime(utc_time);
#else
    return timegm(utc_time);
#endif
}

std::optional<LogTimePoint> build_time_point(const DateAndTime& parsed)
{
    std::tm timestamp {};
    timestamp.tm_year  = parsed.year - 1900;
    timestamp.tm_mon   = static_cast<int>(parsed.month) - 1;
    timestamp.tm_mday  = static_cast<int>(parsed.day);
    timestamp.tm_hour  = static_cast<int>(parsed.hour);
    timestamp.tm_min   = static_cast<int>(parsed.minute);
    timestamp.tm_sec   = static_cast<int>(std::min(parsed.second, 59U));
    timestamp.tm_isdst = -1;

    const bool has_timezone   = parsed.utc_offset_minutes.has_value();
    std::time_t epoch_seconds = has_timezone ? make_utc_time(&timestamp) : std::mktime(&timestamp);
    if (epoch_seconds == static_cast<std::time_t>(-1))
    {
        return std::nullopt;
    }

    if (has_timezone)
    {
        epoch_seconds -= static_cast<std::time_t>(*parsed.utc_offset_minutes) * 60;
    }

    const auto duration = std::chrono::seconds(epoch_seconds) + std::chrono::nanoseconds(parsed.nanosecond);
    return LogTimePoint(std::chrono::duration_cast<LogTimePoint::duration>(duration));
}

bool apply_parser(const eestv::compiledDataAndTimeParser& parser, const std::string& input, int start_index, DateAndTime& output, int& end_index)
{
    std::string to_parse = input;
    int index            = start_index;

    for (const auto& step : parser.dateParser)
    {
        int index_jump = 0;
        if (!step(to_parse, index, index_jump, output))
        {
            return false;
        }

        index += index_jump;
    }

    end_index = index;
    return true;
}

std::string format_two_digits(unsigned value)
{
    std::ostringstream output;
    output << std::setw(2) << std::setfill('0') << value;
    return output.str();
}

std::string format_nine_digits(unsigned value)
{
    std::ostringstream output;
    output << std::setw(9) << std::setfill('0') << value;
    return output.str();
}

std::string trim_fraction_suffix(std::string text)
{
    while (!text.empty() && text.back() == '0')
    {
        text.pop_back();
    }

    return text;
}

std::string format_display_time(const DateAndTime& parsed)
{
    std::ostringstream output;
    output << std::setw(4) << std::setfill('0') << parsed.year << '-' << std::setw(2) << std::setfill('0') << parsed.month << '-' << std::setw(2) << std::setfill('0') << parsed.day << ' ' << std::setw(2) << std::setfill('0') << parsed.hour
           << ':' << std::setw(2) << std::setfill('0') << parsed.minute << ':' << std::setw(2) << std::setfill('0') << parsed.second;

    if (parsed.nanosecond != 0)
    {
        output << '.' << trim_fraction_suffix(format_nine_digits(parsed.nanosecond));
    }

    if (parsed.utc_offset_minutes.has_value())
    {
        const int total_minutes    = *parsed.utc_offset_minutes;
        const int absolute_minutes = std::abs(total_minutes);
        const int hours            = absolute_minutes / 60;
        const int minutes          = absolute_minutes % 60;
        output << (total_minutes >= 0 ? '+' : '-') << format_two_digits(static_cast<unsigned>(hours)) << ':' << format_two_digits(static_cast<unsigned>(minutes));
    }

    return output.str();
}

std::optional<LogLineMetadata> try_parse_with_format(const eestv::compiledDataAndTimeParser& parser, const std::string& line, int start_index)
{
    DateAndTime parsed;
    int end_index = 0;
    if (!apply_parser(parser, line, start_index, parsed, end_index))
    {
        return std::nullopt;
    }

    const auto time_point = build_time_point(parsed);
    if (!time_point.has_value())
    {
        return std::nullopt;
    }

    return LogLineMetadata {
        *time_point,
        line.substr(static_cast<std::size_t>(start_index), static_cast<std::size_t>(end_index - start_index)),
        format_display_time(parsed),
    };
}

const eestv::compiledDataAndTimeParser& compiled_parser_from_entry(const TimestampFormatCatalog::Entry& entry)
{
    return *static_cast<const eestv::compiledDataAndTimeParser*>(entry.compiled_parser.get());
}

std::vector<std::string> sanitize_formats(std::vector<std::string> formats)
{
    formats.erase(std::remove_if(formats.begin(), formats.end(), [](const std::string& format) { return format.empty(); }), formats.end());
    if (formats.empty())
    {
        return default_timestamp_formats();
    }

    return formats;
}

std::shared_ptr<const TimestampFormatCatalog>& mutable_default_catalog()
{
    static auto catalog = std::make_shared<const TimestampFormatCatalog>(default_timestamp_formats());
    return catalog;
}

} // namespace

TimestampFormatCatalog::TimestampFormatCatalog(std::vector<std::string> formats) : _formats(sanitize_formats(std::move(formats)))
{
    TimestampParser parser;
    _entries.reserve(_formats.size());
    for (const auto& format : _formats)
    {
        _entries.push_back(Entry {format, std::make_shared<eestv::compiledDataAndTimeParser>(parser.CompileFormat(format))});
    }
}

TimestampFormatCatalog::~TimestampFormatCatalog() = default;

const std::vector<std::string>& TimestampFormatCatalog::formats() const
{
    return _formats;
}

const std::vector<TimestampFormatCatalog::Entry>& TimestampFormatCatalog::entries() const
{
    return _entries;
}

std::vector<std::string> default_timestamp_formats()
{
    return {
        "YYYY-MM-DDThh:mm:ss.ffffffZZZ", "YYYY-MM-DDThh:mm:ssZZZ", "YYYY-MM-DDThh:mm:ssZZ",   "YYYY-MM-DDThh:mm:ssZ",  "YYYY-MM-DDThh:mm:ss.f",   "YYYY-MM-DDThh:mm:ss.fff",
        "YYYY-MM-DDThh:mm:ss",           "[YYYY-MM-DDThh:mm:ss]",  "YYYY-MM-DD hh:mm:ss",     "[YYYY-MM-DD hh:mm:ss]", "YYYY-MM-DD hh:mm:ss.fff", "YYYY-MM-DD hh:mm:ss,fff",
        "DD-MMM-YYYY hh:mm:ss",          "MMM DD hh:mm:ss",        "DD/MMM/YYYY:hh:mm:ss ZZ", "YYYYMMDDThhmmssZ",      "YYYYMMDDThhmmssZZ",
    };
}

std::shared_ptr<const TimestampFormatCatalog> default_timestamp_format_catalog()
{
    return mutable_default_catalog();
}

void set_default_timestamp_format_catalog(std::shared_ptr<const TimestampFormatCatalog> catalog)
{
    if (catalog == nullptr)
    {
        catalog = std::make_shared<const TimestampFormatCatalog>(default_timestamp_formats());
    }

    mutable_default_catalog() = std::move(catalog);
}

SourceTimestampParser::SourceTimestampParser(std::shared_ptr<const TimestampFormatCatalog> catalog) : _catalog(std::move(catalog))
{
    if (_catalog == nullptr)
    {
        _catalog = default_timestamp_format_catalog();
    }
}

bool SourceTimestampParser::parse(RawLogLine& line)
{
    const auto start_indices = TimestampParser::possible_parse_start_indices(line.text);
    if (start_indices.empty() || _catalog == nullptr)
    {
        return false;
    }

    const auto apply_metadata = [&line](LogLineMetadata&& metadata)
    {
        line.metadata.timestamp           = std::move(metadata.timestamp);
        line.metadata.extracted_time_text = std::move(metadata.extracted_time_text);
        line.metadata.parsed_time_text    = std::move(metadata.parsed_time_text);
    };

    if (_detected_format_index.has_value() && _detected_start_index_slot.has_value())
    {
        if (*_detected_format_index >= _catalog->entries().size() || *_detected_start_index_slot >= start_indices.size())
        {
            return false;
        }

        auto parsed = try_parse_with_format(compiled_parser_from_entry(_catalog->entries()[*_detected_format_index]), line.text, start_indices[*_detected_start_index_slot]);
        if (!parsed.has_value())
        {
            return false;
        }

        apply_metadata(std::move(*parsed));
        return true;
    }

    for (std::size_t start_slot = 0; start_slot < start_indices.size(); ++start_slot)
    {
        const int start_index = start_indices[start_slot];
        for (std::size_t format_index = 0; format_index < _catalog->entries().size(); ++format_index)
        {
            auto parsed = try_parse_with_format(compiled_parser_from_entry(_catalog->entries()[format_index]), line.text, start_index);
            if (!parsed.has_value())
            {
                continue;
            }

            _detected_format_index     = format_index;
            _detected_start_index_slot = start_slot;
            apply_metadata(std::move(*parsed));
            return true;
        }
    }

    return false;
}

std::optional<LogLineMetadata> parse_log_timestamp_details(const std::string& line)
{
    SourceTimestampParser parser;
    RawLogLine raw_line(line);
    if (!parser.parse(raw_line))
    {
        return std::nullopt;
    }

    return raw_line.metadata;
}

std::optional<LogTimePoint> parse_log_timestamp(const std::string& line)
{
    const auto parsed = parse_log_timestamp_details(line);
    if (!parsed.has_value())
    {
        return std::nullopt;
    }

    return parsed->timestamp;
}

} // namespace slayerlog
