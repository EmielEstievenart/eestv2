#include "log_batch.hpp"

#include <cassert>
#include <cstddef>
#include <optional>
#include <vector>

#include "log_timestamp.hpp"

namespace slayerlog
{

namespace
{

struct WatcherState
{
    std::size_t next_line_index = 0;
    bool current_timestamp_parsed = false;
    std::optional<LogTimePoint> current_timestamp;
};

bool has_pending_line(const WatcherLineBatch& watcher_batch, const WatcherState& watcher_state)
{
    return watcher_state.next_line_index < watcher_batch.size();
}

std::optional<LogTimePoint> get_current_timestamp(const WatcherLineBatch& watcher_batch, WatcherState& watcher_state)
{
    if (!watcher_state.current_timestamp_parsed && has_pending_line(watcher_batch, watcher_state))
    {
        watcher_state.current_timestamp = parse_log_timestamp(watcher_batch[watcher_state.next_line_index]);
        watcher_state.current_timestamp_parsed = true;
    }

    return watcher_state.current_timestamp;
}

void advance_watcher(WatcherState& watcher_state)
{
    ++watcher_state.next_line_index;
    watcher_state.current_timestamp.reset();
    watcher_state.current_timestamp_parsed = false;
}

} // namespace

std::vector<ObservedLogLine> merge_log_batch(
    const std::vector<WatcherLineBatch>& watcher_batches,
    const std::vector<std::string>& source_labels)
{
    assert(source_labels.size() == watcher_batches.size());

    std::size_t total_line_count = 0;
    for (const auto& watcher_batch : watcher_batches)
    {
        total_line_count += watcher_batch.size();
    }

    std::vector<ObservedLogLine> merged_lines;
    merged_lines.reserve(total_line_count);

    std::vector<WatcherState> watcher_states(watcher_batches.size());

    while (merged_lines.size() < total_line_count)
    {
        for (std::size_t watcher_index = 0; watcher_index < watcher_batches.size(); ++watcher_index)
        {
            const auto& watcher_batch = watcher_batches[watcher_index];
            auto& watcher_state = watcher_states[watcher_index];

            while (has_pending_line(watcher_batch, watcher_state))
            {
                const auto current_timestamp = get_current_timestamp(watcher_batch, watcher_state);
                if (current_timestamp.has_value())
                {
                    break;
                }

                merged_lines.push_back({
                    source_labels[watcher_index],
                    watcher_batch[watcher_state.next_line_index],
                });
                advance_watcher(watcher_state);
            }
        }

        if (merged_lines.size() >= total_line_count)
        {
            break;
        }

        std::optional<std::size_t> next_watcher_index;
        std::optional<LogTimePoint> next_timestamp;

        for (std::size_t watcher_index = 0; watcher_index < watcher_batches.size(); ++watcher_index)
        {
            const auto& watcher_batch = watcher_batches[watcher_index];
            auto& watcher_state = watcher_states[watcher_index];
            if (!has_pending_line(watcher_batch, watcher_state))
            {
                continue;
            }

            const auto current_timestamp = get_current_timestamp(watcher_batch, watcher_state);
            if (!current_timestamp.has_value())
            {
                continue;
            }

            if (!next_watcher_index.has_value()
                || current_timestamp.value() < next_timestamp.value())
            {
                next_watcher_index = watcher_index;
                next_timestamp = current_timestamp;
            }
        }

        if (!next_watcher_index.has_value())
        {
            break;
        }

        auto& next_watcher_state = watcher_states[next_watcher_index.value()];
        merged_lines.push_back({
            source_labels[next_watcher_index.value()],
            watcher_batches[next_watcher_index.value()][next_watcher_state.next_line_index],
        });
        advance_watcher(next_watcher_state);
    }

    return merged_lines;
}

} // namespace slayerlog
