#include "all_tracked_sources.hpp"

#include <cstddef>
#include <exception>
#include <memory>
#include <utility>

#include "debug_log.hpp"
#include "tracked_source_base.hpp"
#include "tracked_source_factory.hpp"

namespace slayerlog
{

AllTrackedSources::AllTrackedSources(std::shared_ptr<const TimestampFormatCatalog> timestamp_formats) : _timestamp_formats(std::move(timestamp_formats))
{
    if (_timestamp_formats == nullptr)
    {
        _timestamp_formats = default_timestamp_format_catalog();
    }
}

AllTrackedSources::~AllTrackedSources() = default;

std::optional<std::string> AllTrackedSources::open_source(const LogSource& source)
{
    if (contains_source(source))
    {
        return "Source already open: " + source_display_path(source);
    }

    try
    {
        const std::size_t source_index = _sources.size();
        auto source_state              = create_tracked_source(source, source_display_path(source), _timestamp_formats);

        source_state->poll();

        _sources.push_back(std::move(source_state));
        rebuild_source_labels();
        rebuild_all_lines();

        SLAYERLOG_LOG_INFO("Opened source index=" << source_index << " source=" << source_display_path(source));
        return std::nullopt;
    }
    catch (const std::exception& ex)
    {
        return ex.what();
    }
}

std::optional<std::string> AllTrackedSources::close_source(std::size_t source_index, std::string* closed_label)
{
    if (source_index >= _sources.size())
    {
        return "Invalid open file selection";
    }

    if (closed_label != nullptr)
    {
        *closed_label = _sources[source_index]->source_label();
    }

    _sources.erase(_sources.begin() + static_cast<std::ptrdiff_t>(source_index));
    rebuild_source_labels();
    rebuild_all_lines();
    return std::nullopt;
}

std::optional<AllLineIndex> AllTrackedSources::poll()
{
    std::vector<LogBatchSourceRange> source_ranges;
    source_ranges.reserve(_sources.size());

    for (std::size_t source_index = 0; source_index < _sources.size(); ++source_index)
    {
        auto& source = _sources[source_index];

        try
        {
            const std::size_t first_new_entry_index = source->entries().size();

            if (!source->poll())
            {
                continue;
            }

            append_source_range(source_ranges, *source, source_index, first_new_entry_index);
        }
        catch (const std::exception& ex)
        {
            SLAYERLOG_LOG_WARNING("Watcher poll threw for source=" << source_display_path(source->source()) << " error=" << ex.what());
            continue;
        }
        catch (...)
        {
            SLAYERLOG_LOG_WARNING("Watcher poll threw for source=" << source_display_path(source->source()) << " error=<unknown>");
            continue;
        }
    }

    if (source_ranges.empty())
    {
        return std::nullopt;
    }

    const AllLineIndex first_new_index {static_cast<int>(_all_lines.size())};
    append_merged_lines(merge_log_batch(source_ranges));
    return first_new_index;
}

const IndexedVector<std::shared_ptr<LogEntry>, AllLineIndex>& AllTrackedSources::all_lines() const
{
    return _all_lines;
}

int AllTrackedSources::line_count() const
{
    return static_cast<int>(_all_lines.size());
}

std::size_t AllTrackedSources::source_count() const
{
    return _sources.size();
}

bool AllTrackedSources::empty() const
{
    return _sources.empty();
}

std::vector<std::string> AllTrackedSources::source_labels() const
{
    std::vector<std::string> labels;
    labels.reserve(_sources.size());
    for (const auto& source : _sources)
    {
        labels.push_back(source->source_label());
    }

    return labels;
}

bool AllTrackedSources::contains_source(const LogSource& candidate_source) const
{
    for (const auto& source : _sources)
    {
        if (same_source(source->source(), candidate_source))
        {
            return true;
        }
    }

    return false;
}

void AllTrackedSources::rebuild_source_labels()
{
    std::vector<LogSource> sources;
    sources.reserve(_sources.size());
    for (const auto& source : _sources)
    {
        sources.push_back(source->source());
    }

    const auto labels = build_source_labels(sources);
    for (std::size_t index = 0; index < _sources.size(); ++index)
    {
        _sources[index]->set_source_label(labels[index]);
    }
}

void AllTrackedSources::rebuild_all_lines()
{
    _all_lines.clear();

    std::vector<LogBatchSourceRange> source_ranges;
    source_ranges.reserve(_sources.size());
    for (std::size_t source_index = 0; source_index < _sources.size(); ++source_index)
    {
        append_source_range(source_ranges, *_sources[source_index], source_index, 0);
    }

    append_merged_lines(merge_log_batch(source_ranges));
}

void AllTrackedSources::append_source_range(std::vector<LogBatchSourceRange>& source_ranges, const TrackedSourceBase& source, std::size_t source_index,
                                            std::size_t first_entry_index) const
{
    const auto& entries = source.entries();
    if (first_entry_index >= entries.size())
    {
        return;
    }

    source_ranges.push_back({
        &entries,
        first_entry_index,
        source_index,
        source.source_label(),
    });
}

void AllTrackedSources::append_merged_lines(const std::vector<std::shared_ptr<LogEntry>>& lines)
{
    _all_lines.reserve(_all_lines.size() + lines.size());
    for (const auto& line : lines)
    {
        _all_lines.push_back(line);
    }
}

} // namespace slayerlog
