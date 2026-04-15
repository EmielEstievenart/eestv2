#include "all_tracked_sources.hpp"

#include <cstddef>
#include <exception>
#include <memory>
#include <utility>

#include "debug_log.hpp"
#include "tracked_source.hpp"
#include "watchers/file_watcher.hpp"
#include "watchers/folder_watcher.hpp"
#include "watchers/log_watcher_base.hpp"
#include "watchers/ssh_tail_watcher.hpp"

namespace slayerlog
{

struct AllTrackedSources::SourceState
{
    std::unique_ptr<LogWatcherBase> watcher;
    TrackedSource tracked_source;
};

AllTrackedSources::AllTrackedSources(std::shared_ptr<const TimestampFormatCatalog> timestamp_formats) : _timestamp_formats(std::move(timestamp_formats))
{
    if (_timestamp_formats == nullptr)
    {
        _timestamp_formats = default_timestamp_format_catalog();
    }
}

AllTrackedSources::~AllTrackedSources() = default;

std::optional<std::string> AllTrackedSources::open_source(std::string_view source_spec)
{
    try
    {
        return open_source(parse_log_source(source_spec));
    }
    catch (const std::exception& ex)
    {
        return ex.what();
    }
}

std::optional<std::string> AllTrackedSources::open_source(const LogSource& source)
{
    if (contains_source(source))
    {
        return "Source already open: " + source_display_path(source);
    }

    try
    {
        auto watcher = create_watcher_for_source(source);

        const std::size_t source_index = _sources.size();
        SourceState source_state {
            std::move(watcher),
            TrackedSource(source, source_display_path(source), _timestamp_formats),
        };

        if (auto* folder_watcher = dynamic_cast<FolderWatcher*>(source_state.watcher.get()))
        {
            source_state.tracked_source.add_entries(folder_watcher->poll_parsed_lines());
        }
        else
        {
            std::vector<std::string> lines;
            source_state.watcher->poll(lines);
            source_state.tracked_source.add_entries_from_raw_strings(lines);
        }

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
        *closed_label = _sources[source_index].tracked_source.source_label();
    }

    _sources.erase(_sources.begin() + static_cast<std::ptrdiff_t>(source_index));
    rebuild_source_labels();
    rebuild_all_lines();
    return std::nullopt;
}

std::optional<AllLineIndex> AllTrackedSources::poll()
{
    LogBatch batch;

    for (std::size_t source_index = 0; source_index < _sources.size(); ++source_index)
    {
        auto& source_state = _sources[source_index];

        try
        {
            const std::size_t first_new_entry_index = source_state.tracked_source.entries().size();

            if (auto* folder_watcher = dynamic_cast<FolderWatcher*>(source_state.watcher.get()))
            {
                auto lines = folder_watcher->poll_parsed_lines();
                if (lines.empty())
                {
                    continue;
                }

                source_state.tracked_source.add_entries(std::move(lines));
                append_entries_to_batch(batch, source_state, source_index, first_new_entry_index);
                continue;
            }

            std::vector<std::string> lines;
            source_state.watcher->poll(lines);
            if (lines.empty())
            {
                continue;
            }

            source_state.tracked_source.add_entries_from_raw_strings(lines);
            append_entries_to_batch(batch, source_state, source_index, first_new_entry_index);
        }
        catch (const std::exception& ex)
        {
            SLAYERLOG_LOG_WARNING("Watcher poll threw for source=" << source_display_path(source_state.tracked_source.source()) << " error=" << ex.what());
            continue;
        }
        catch (...)
        {
            SLAYERLOG_LOG_WARNING("Watcher poll threw for source=" << source_display_path(source_state.tracked_source.source()) << " error=<unknown>");
            continue;
        }
    }

    if (batch.empty())
    {
        return std::nullopt;
    }

    const AllLineIndex first_new_index {static_cast<int>(_all_lines.size())};
    append_merged_lines(merge_log_batch(batch));
    return first_new_index;
}

const IndexedVector<ObservedLogLine, AllLineIndex>& AllTrackedSources::all_lines() const
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
    for (const auto& source_state : _sources)
    {
        labels.push_back(source_state.tracked_source.source_label());
    }

    return labels;
}

std::unique_ptr<LogWatcherBase> AllTrackedSources::create_watcher_for_source(const LogSource& source) const
{
    if (source.kind == LogSourceKind::SshRemoteFile)
    {
        return std::make_unique<SshTailWatcher>(source);
    }

    if (source.kind == LogSourceKind::LocalFolder)
    {
        return std::make_unique<FolderWatcher>(source.local_folder_path, _timestamp_formats);
    }

    return std::make_unique<FileWatcher>(source.local_path);
}

bool AllTrackedSources::contains_source(const LogSource& candidate_source) const
{
    for (const auto& source_state : _sources)
    {
        if (same_source(source_state.tracked_source.source(), candidate_source))
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
    for (const auto& source_state : _sources)
    {
        sources.push_back(source_state.tracked_source.source());
    }

    const auto labels = build_source_labels(sources);
    for (std::size_t index = 0; index < _sources.size(); ++index)
    {
        _sources[index].tracked_source.set_source_label(labels[index]);
    }
}

void AllTrackedSources::rebuild_all_lines()
{
    _all_lines.clear();

    LogBatch batch;
    for (std::size_t source_index = 0; source_index < _sources.size(); ++source_index)
    {
        append_entries_to_batch(batch, _sources[source_index], source_index, 0);
    }

    append_merged_lines(merge_log_batch(batch));
}

void AllTrackedSources::append_entries_to_batch(LogBatch& batch, const SourceState& source_state, std::size_t source_index, std::size_t first_entry_index) const
{
    const auto& entries = source_state.tracked_source.entries();
    if (first_entry_index >= entries.size())
    {
        return;
    }

    batch.reserve(batch.size() + (entries.size() - first_entry_index));
    for (std::size_t entry_index = first_entry_index; entry_index < entries.size(); ++entry_index)
    {
        const auto& entry = entries[entry_index];
        LogBatchEntry batch_entry;
        batch_entry.source_index           = source_index;
        batch_entry.source_label           = source_state.tracked_source.source_label();
        batch_entry.text                   = entry.raw_text;
        batch_entry.timestamp              = entry.timestamp;
        batch_entry.source_sequence_number = entry.sequence_number;
        batch_entry.parsed_time_text       = entry.parsed_timestamp_text;
        batch.push_back(std::move(batch_entry));
    }
}

void AllTrackedSources::append_merged_lines(const std::vector<ObservedLogLine>& lines)
{
    _all_lines.reserve(_all_lines.size() + lines.size());
    for (const auto& line : lines)
    {
        _all_lines.push_back(line);
    }
}

} // namespace slayerlog
