#include "log_view_model.hpp"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <utility>
#include <vector>

#include "log_timestamp.hpp"

namespace slayerlog
{

namespace
{

bool is_before(const TextPosition& lhs, const TextPosition& rhs)
{
    return lhs.line < rhs.line || (lhs.line == rhs.line && lhs.column < rhs.column);
}

struct LineGroup
{
    std::vector<std::string> lines;
    std::optional<LogTimePoint> anchor_timestamp;
    int source_group_order = 0;
};

std::vector<LineGroup> build_line_groups(const std::vector<std::string>& lines)
{
    std::vector<LineGroup> groups;
    groups.reserve(lines.size());

    std::vector<std::string> prefix_lines;
    std::optional<std::size_t> current_group_index;
    int next_group_order = 0;

    for (const auto& line : lines)
    {
        auto timestamp = parse_log_timestamp(line);
        if (timestamp.has_value())
        {
            LineGroup group;
            group.anchor_timestamp = *timestamp;
            group.source_group_order = next_group_order++;
            group.lines.push_back(line);
            groups.push_back(std::move(group));
            current_group_index = groups.size() - 1;
            continue;
        }

        if (current_group_index.has_value())
        {
            groups[*current_group_index].lines.push_back(line);
        }
        else
        {
            prefix_lines.push_back(line);
        }
    }

    if (groups.empty())
    {
        if (lines.empty())
        {
            return {};
        }

        LineGroup block;
        block.lines = lines;
        groups.push_back(std::move(block));
        return groups;
    }

    if (!prefix_lines.empty())
    {
        LineGroup prefix_group;
        prefix_group.lines = std::move(prefix_lines);
        prefix_group.anchor_timestamp = groups.front().anchor_timestamp;
        prefix_group.source_group_order = -1;
        groups.insert(groups.begin(), std::move(prefix_group));
    }

    return groups;
}

} // namespace

void LogViewModel::apply_initial_updates(const std::vector<ObservedLogUpdate>& updates)
{
    for (const auto& update : updates)
    {
        if (update.kind == FileWatcher::Update::Kind::Snapshot)
        {
            _entries.erase(
                std::remove_if(
                    _entries.begin(),
                    _entries.end(),
                    [&](const LogEntry& entry)
                    {
                        return entry.source_path == update.source_path;
                    }),
                _entries.end());
        }

        append_entries_for_update(update);
    }

    sort_entries();
    clamp_selection();
    clamp_scroll_offset();
    if (_follow_bottom)
    {
        _scroll_offset = max_scroll_offset();
    }
    update_follow_bottom();
}

void LogViewModel::apply_update(const ObservedLogUpdate& update)
{
    if (_updates_paused)
    {
        _paused_updates.push_back(update);
    }
    else
    {
        apply_update_immediately(update);
    }

    clamp_selection();
    clamp_scroll_offset();
    update_follow_bottom();
}

void LogViewModel::toggle_pause()
{
    _updates_paused = !_updates_paused;
    if (!_updates_paused)
    {
        flush_paused_updates();
        clamp_selection();
        clamp_scroll_offset();
        update_follow_bottom();
    }
}

bool LogViewModel::updates_paused() const
{
    return _updates_paused;
}

void LogViewModel::set_show_source_labels(bool show_source_labels)
{
    _show_source_labels = show_source_labels;
    clamp_selection();
}

void LogViewModel::set_visible_line_count(int count)
{
    _visible_line_count = std::max(1, count);
    if (_follow_bottom)
    {
        _scroll_offset = max_scroll_offset();
    }
    else
    {
        clamp_scroll_offset();
        update_follow_bottom();
    }
}

int LogViewModel::visible_line_count() const
{
    return _visible_line_count;
}

int LogViewModel::scroll_offset() const
{
    return _scroll_offset;
}

int LogViewModel::line_count() const
{
    return static_cast<int>(_entries.size());
}

void LogViewModel::scroll_up(int amount)
{
    _scroll_offset -= std::max(1, amount);
    clamp_scroll_offset();
    update_follow_bottom();
}

void LogViewModel::scroll_down(int amount)
{
    _scroll_offset += std::max(1, amount);
    clamp_scroll_offset();
    update_follow_bottom();
}

void LogViewModel::scroll_to_top()
{
    _scroll_offset = 0;
    update_follow_bottom();
}

void LogViewModel::scroll_to_bottom()
{
    _scroll_offset = max_scroll_offset();
    update_follow_bottom();
}

void LogViewModel::begin_selection(TextPosition position)
{
    _selection_anchor      = position;
    _selection_focus       = position;
    _selection_in_progress = true;
    clamp_selection();
}

void LogViewModel::update_selection(TextPosition position)
{
    if (!_selection_in_progress || !_selection_anchor.has_value())
    {
        return;
    }

    _selection_focus = position;
    clamp_selection();
}

void LogViewModel::end_selection(std::optional<TextPosition> position)
{
    _selection_in_progress = false;
    if (position.has_value() && _selection_anchor.has_value())
    {
        _selection_focus = *position;
        clamp_selection();
    }
}

void LogViewModel::clear_selection()
{
    _selection_anchor.reset();
    _selection_focus.reset();
    _selection_in_progress = false;
}

bool LogViewModel::selection_in_progress() const
{
    return _selection_in_progress;
}

std::optional<std::pair<TextPosition, TextPosition>> LogViewModel::selection_bounds() const
{
    if (!_selection_anchor.has_value() || !_selection_focus.has_value())
    {
        return std::nullopt;
    }

    auto start = *_selection_anchor;
    auto end   = *_selection_focus;
    if (is_before(end, start))
    {
        std::swap(start, end);
    }

    return std::pair(start, end);
}

std::string LogViewModel::selection_text() const
{
    const auto bounds = selection_bounds();
    if (!bounds.has_value() || _entries.empty())
    {
        return {};
    }

    const auto [start, end] = *bounds;
    std::ostringstream output;
    for (int line_index = start.line; line_index <= end.line; ++line_index)
    {
        const auto line            = rendered_line(line_index);
        const int line_start       = (line_index == start.line) ? start.column : 0;
        const int line_end         = (line_index == end.line) ? end.column : static_cast<int>(line.size());
        const int clamped_start    = std::clamp(line_start, 0, static_cast<int>(line.size()));
        const int clamped_end      = std::clamp(line_end, clamped_start, static_cast<int>(line.size()));
        const auto selection_count = static_cast<std::size_t>(clamped_end - clamped_start);

        output << line.substr(static_cast<std::size_t>(clamped_start), selection_count);
        if (line_index != end.line)
        {
            output << '\n';
        }
    }

    return output.str();
}

std::string LogViewModel::rendered_line(int index) const
{
    std::ostringstream output;
    output << index + 1 << " ";
    const auto& entry = _entries[static_cast<std::size_t>(index)];
    if (_show_source_labels)
    {
        output << "[" << entry.source_label << "] ";
    }

    output << entry.text;
    return output.str();
}

void LogViewModel::append_entries_for_update(const ObservedLogUpdate& update)
{
    const auto groups = build_line_groups(update.lines);
    for (const auto& group : groups)
    {
        const SortPhase phase = update.is_initial_load
                                    ? (group.anchor_timestamp.has_value() ? SortPhase::InitialTimestamped
                                                                          : SortPhase::InitialUntimestamped)
                                    : SortPhase::Live;

        for (std::size_t line_index = 0; line_index < group.lines.size(); ++line_index)
        {
            _entries.push_back(LogEntry{
                update.source_path,
                update.source_label,
                group.lines[line_index],
                phase,
                group.anchor_timestamp,
                update.source_index,
                group.source_group_order,
                static_cast<int>(line_index),
                update.poll_epoch,
                _next_sequence++,
            });
        }
    }
}

void LogViewModel::apply_update_immediately(const ObservedLogUpdate& update)
{
    if (update.kind == FileWatcher::Update::Kind::Snapshot)
    {
        _entries.erase(
            std::remove_if(
                _entries.begin(),
                _entries.end(),
                [&](const LogEntry& entry)
                {
                    return entry.source_path == update.source_path;
                }),
            _entries.end());
    }

    append_entries_for_update(update);
    sort_entries();
    if (_follow_bottom)
    {
        _scroll_offset = max_scroll_offset();
    }
}

void LogViewModel::flush_paused_updates()
{
    for (const auto& update : _paused_updates)
    {
        apply_update_immediately(update);
    }

    _paused_updates.clear();
}

void LogViewModel::sort_entries()
{
    std::sort(
        _entries.begin(),
        _entries.end(),
        [](const LogEntry& lhs, const LogEntry& rhs)
        {
            const auto phase_rank = [](SortPhase phase)
            {
                switch (phase)
                {
                case SortPhase::InitialTimestamped:
                    return 0;
                case SortPhase::InitialUntimestamped:
                    return 1;
                case SortPhase::Live:
                    return 2;
                }

                return 3;
            };
            const auto batch_kind_rank = [](const LogEntry& entry)
            {
                return entry.anchor_timestamp.has_value() ? 0 : 1;
            };

            const int lhs_phase_rank = phase_rank(lhs.phase);
            const int rhs_phase_rank = phase_rank(rhs.phase);
            if (lhs_phase_rank != rhs_phase_rank)
            {
                return lhs_phase_rank < rhs_phase_rank;
            }

            if (lhs.phase == SortPhase::Live && lhs.poll_epoch != rhs.poll_epoch)
            {
                return lhs.poll_epoch < rhs.poll_epoch;
            }

            if (lhs.phase != SortPhase::InitialUntimestamped)
            {
                const int lhs_batch_rank = batch_kind_rank(lhs);
                const int rhs_batch_rank = batch_kind_rank(rhs);
                if (lhs_batch_rank != rhs_batch_rank)
                {
                    return lhs_batch_rank < rhs_batch_rank;
                }

                if (lhs.anchor_timestamp.has_value() && rhs.anchor_timestamp.has_value() &&
                    lhs.anchor_timestamp != rhs.anchor_timestamp)
                {
                    return lhs.anchor_timestamp < rhs.anchor_timestamp;
                }
            }

            if (lhs.source_index != rhs.source_index)
            {
                return lhs.source_index < rhs.source_index;
            }

            if (lhs.source_group_order != rhs.source_group_order)
            {
                return lhs.source_group_order < rhs.source_group_order;
            }

            if (lhs.line_order != rhs.line_order)
            {
                return lhs.line_order < rhs.line_order;
            }

            return lhs.sequence < rhs.sequence;
        });
}

void LogViewModel::clamp_scroll_offset()
{
    _scroll_offset = std::clamp(_scroll_offset, 0, max_scroll_offset());
}

int LogViewModel::max_scroll_offset() const
{
    return std::max(0, static_cast<int>(_entries.size()) - _visible_line_count);
}

void LogViewModel::update_follow_bottom()
{
    _follow_bottom = _scroll_offset >= max_scroll_offset();
}

void LogViewModel::clamp_selection()
{
    if (_entries.empty())
    {
        clear_selection();
        return;
    }

    auto clamp_position = [&](TextPosition& position)
    {
        position.line = std::clamp(position.line, 0, static_cast<int>(_entries.size()) - 1);
        const auto line_length = static_cast<int>(rendered_line(position.line).size());
        position.column        = std::clamp(position.column, 0, line_length);
    };

    if (_selection_anchor.has_value())
    {
        clamp_position(*_selection_anchor);
    }

    if (_selection_focus.has_value())
    {
        clamp_position(*_selection_focus);
    }
}

} // namespace slayerlog
