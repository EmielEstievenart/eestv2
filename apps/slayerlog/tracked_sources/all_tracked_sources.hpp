#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "log_batch.hpp"
#include "log_source.hpp"
#include "log_timestamp.hpp"
#include "log_types.hpp"

namespace slayerlog
{

class TrackedSource;

class AllTrackedSources
{
public:
    explicit AllTrackedSources(std::shared_ptr<const TimestampFormatCatalog> timestamp_formats = default_timestamp_format_catalog());
    ~AllTrackedSources();

    std::optional<std::string> open_source(std::string_view source_spec);
    std::optional<std::string> open_source(const LogSource& source);
    std::optional<std::string> close_source(std::size_t source_index, std::string* closed_label = nullptr);

    std::optional<AllLineIndex> poll();

    const IndexedVector<ObservedLogLine, AllLineIndex>& all_lines() const;
    int line_count() const;

    std::size_t source_count() const;
    bool empty() const;
    std::vector<std::string> source_labels() const;

private:
    struct SourceState;

    bool contains_source(const LogSource& candidate_source) const;
    void rebuild_source_labels();
    void rebuild_all_lines();
    void append_entries_to_batch(LogBatch& batch, const SourceState& source_state, std::size_t source_index, std::size_t first_entry_index) const;
    void append_merged_lines(const std::vector<ObservedLogLine>& lines);

    std::vector<SourceState> _sources;
    IndexedVector<ObservedLogLine, AllLineIndex> _all_lines;
    std::shared_ptr<const TimestampFormatCatalog> _timestamp_formats;
};

} // namespace slayerlog
