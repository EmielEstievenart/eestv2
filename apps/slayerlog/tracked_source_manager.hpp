#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "log_batch.hpp"
#include "log_source.hpp"

namespace slayerlog
{

class LogWatcherBase;

class TrackedSourceManager
{
public:
    TrackedSourceManager();
    ~TrackedSourceManager();

    /** @brief Parses and opens a new source, creating its watcher and initial tracked storage. */
    std::optional<std::string> open_source(std::string_view source_spec);
    /** @brief Opens a new source from an already parsed descriptor. */
    std::optional<std::string> open_source(const LogSource& source);
    /** @brief Closes one tracked source by index and optionally returns the label that was removed. */
    std::optional<std::string> close_source(std::size_t source_index, std::string* closed_label = nullptr);

    /** @brief Polls all watchers and returns only newly observed entries since the last poll/open. */
    LogBatch poll();
    /** @brief Returns the full stored contents of every tracked source in its current state. */
    LogBatch snapshot() const;

    std::size_t source_count() const;
    bool empty() const;
    std::vector<std::string> source_labels() const;

private:
    struct SourceState;

    static std::unique_ptr<LogWatcherBase> create_watcher_for_source(const LogSource& source);
    bool contains_source(const LogSource& candidate_source) const;
    void rebuild_source_labels();
    void append_entries_to_batch(LogBatch& batch, const SourceState& source_state, std::size_t source_index, std::size_t first_entry_index) const;

    std::vector<SourceState> _sources;
};

} // namespace slayerlog
