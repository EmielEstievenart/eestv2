#include "tracked_source_manager.hpp"

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

struct TrackedSourceManager::SourceState
{
    std::unique_ptr<LogWatcherBase> watcher;
    TrackedSource tracked_source;
};

TrackedSourceManager::TrackedSourceManager()  = default;
TrackedSourceManager::~TrackedSourceManager() = default;

std::optional<std::string> TrackedSourceManager::open_source(std::string_view source_spec)
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

std::optional<std::string> TrackedSourceManager::open_source(const LogSource& source)
{
    if (contains_source(source))
    {
        return "Source already open: " + source_display_path(source);
    }

    try
    {
        auto watcher = create_watcher_for_source(source);
        std::vector<std::string> lines;
        watcher->poll(lines);

        const std::size_t source_index = _sources.size();
        SourceState source_state {
            std::move(watcher),
            TrackedSource(source, source_display_path(source)),
        };
        source_state.tracked_source.add_entries_from_raw_strings(lines);

        _sources.push_back(std::move(source_state));
        rebuild_source_labels();

        SLAYERLOG_LOG_INFO("Opened source index=" << source_index << " source=" << source_display_path(source));
        return std::nullopt;
    }
    catch (const std::exception& ex)
    {
        return ex.what();
    }
}

std::optional<std::string> TrackedSourceManager::close_source(std::size_t source_index, std::string* closed_label)
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
    return std::nullopt;
}

LogBatch TrackedSourceManager::poll()
{
    LogBatch batch;

    for (std::size_t source_index = 0; source_index < _sources.size(); ++source_index)
    {
        auto& source_state = _sources[source_index];
        std::vector<std::string> lines;

        try
        {
            source_state.watcher->poll(lines);
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

        if (lines.empty())
        {
            continue;
        }

        const std::size_t first_new_entry_index = source_state.tracked_source.entries().size();
        source_state.tracked_source.add_entries_from_raw_strings(lines);
        append_entries_to_batch(batch, source_state, source_index, first_new_entry_index);
    }

    return batch;
}

LogBatch TrackedSourceManager::snapshot() const
{
    LogBatch batch;
    for (std::size_t source_index = 0; source_index < _sources.size(); ++source_index)
    {
        append_entries_to_batch(batch, _sources[source_index], source_index, 0);
    }

    return batch;
}

std::size_t TrackedSourceManager::source_count() const
{
    return _sources.size();
}

bool TrackedSourceManager::empty() const
{
    return _sources.empty();
}

std::vector<std::string> TrackedSourceManager::source_labels() const
{
    std::vector<std::string> labels;
    labels.reserve(_sources.size());
    for (const auto& source_state : _sources)
    {
        labels.push_back(source_state.tracked_source.source_label());
    }

    return labels;
}

std::unique_ptr<LogWatcherBase> TrackedSourceManager::create_watcher_for_source(const LogSource& source)
{
    if (source.kind == LogSourceKind::SshRemoteFile)
    {
        return std::make_unique<SshTailWatcher>(source);
    }

    if (source.kind == LogSourceKind::LocalFolder)
    {
        return std::make_unique<FolderWatcher>(source.local_folder_path);
    }

    return std::make_unique<FileWatcher>(source.local_path);
}

bool TrackedSourceManager::contains_source(const LogSource& candidate_source) const
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

void TrackedSourceManager::rebuild_source_labels()
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

void TrackedSourceManager::append_entries_to_batch(LogBatch& batch, const SourceState& source_state, std::size_t source_index, std::size_t first_entry_index) const
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
        batch.push_back(LogBatchEntry {
            source_index,
            source_state.tracked_source.source_label(),
            entry.raw_text,
            entry.timestamp,
            entry.sequence_number,
        });
    }
}

} // namespace slayerlog
