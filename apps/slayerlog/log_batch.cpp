#include "log_batch.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <optional>
#include <vector>

namespace slayerlog
{

namespace
{

struct SourceBatchState
{
    std::vector<const LogEntry*> entries;
    std::size_t next_entry_index = 0;
};

bool has_pending_entry(const SourceBatchState& state)
{
    return state.next_entry_index < state.entries.size();
}

const LogEntry& current_entry(const SourceBatchState& state)
{
    return *state.entries[state.next_entry_index];
}

void advance_source(SourceBatchState& state)
{
    ++state.next_entry_index;
}

} // namespace

std::vector<LogEntry> merge_log_batch(const std::vector<LogEntry>& batch)
{
    std::size_t highest_source_index = 0;
    for (const auto& entry : batch)
    {
        highest_source_index = std::max(highest_source_index, entry.metadata.source_index);
    }

    std::vector<SourceBatchState> source_states(batch.empty() ? 0 : highest_source_index + 1);
    for (const auto& entry : batch)
    {
        source_states[entry.metadata.source_index].entries.push_back(&entry);
    }

    std::vector<LogEntry> merged_lines;
    merged_lines.reserve(batch.size());

    while (merged_lines.size() < batch.size())
    {
        for (auto& source_state : source_states)
        {
            while (has_pending_entry(source_state))
            {
                const auto& entry = current_entry(source_state);
                if (entry.metadata.timestamp.has_value())
                {
                    break;
                }

                merged_lines.push_back(entry);
                advance_source(source_state);
            }
        }

        if (merged_lines.size() >= batch.size())
        {
            break;
        }

        std::optional<std::size_t> next_source_index;
        std::optional<std::chrono::system_clock::time_point> next_timestamp;

        for (std::size_t source_index = 0; source_index < source_states.size(); ++source_index)
        {
            const auto& source_state = source_states[source_index];
            if (!has_pending_entry(source_state))
            {
                continue;
            }

            const auto& entry = current_entry(source_state);
            if (!entry.metadata.timestamp.has_value())
            {
                continue;
            }

            if (!next_source_index.has_value() || entry.metadata.timestamp.value() < next_timestamp.value())
            {
                next_source_index = source_index;
                next_timestamp    = entry.metadata.timestamp;
            }
        }

        if (!next_source_index.has_value())
        {
            break;
        }

        auto& source_state = source_states[*next_source_index];
        const auto& entry  = current_entry(source_state);
        merged_lines.push_back(entry);
        advance_source(source_state);
    }

    return merged_lines;
}

} // namespace slayerlog
