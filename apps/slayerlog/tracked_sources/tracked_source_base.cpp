#include "tracked_source_base.hpp"

#include <utility>

namespace slayerlog
{

TrackedSourceBase::TrackedSourceBase(LogSource source, std::string source_label, std::shared_ptr<const TimestampFormatCatalog> timestamp_formats)
    : _source(std::move(source)), _source_label(std::move(source_label)), _timestamp_formats(std::move(timestamp_formats))
{
    if (_timestamp_formats == nullptr)
    {
        _timestamp_formats = default_timestamp_format_catalog();
    }
}

const LogSource& TrackedSourceBase::source() const
{
    return _source;
}

const std::string& TrackedSourceBase::source_label() const
{
    return _source_label;
}

void TrackedSourceBase::set_source_label(std::string source_label)
{
    _source_label = std::move(source_label);
}

const std::vector<LogEntry>& TrackedSourceBase::entries() const
{
    return _entries;
}

const std::shared_ptr<const TimestampFormatCatalog>& TrackedSourceBase::timestamp_formats() const
{
    return _timestamp_formats;
}

void TrackedSourceBase::reserve_entries(std::size_t additional_count)
{
    _entries.reserve(_entries.size() + additional_count);
}

LogEntry& TrackedSourceBase::append_entry()
{
    LogEntry& entry          = _entries.emplace_back();
    entry.metadata.sequence_number = _next_sequence_number++;
    entry.metadata.source          = this;
    return entry;
}

} // namespace slayerlog
