#include "log_view_model.hpp"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <utility>

namespace slayerlog
{

namespace
{

bool is_before(const TextPosition& lhs, const TextPosition& rhs)
{
    return lhs.line < rhs.line || (lhs.line == rhs.line && lhs.column < rhs.column);
}

} // namespace

void LogViewModel::append_lines(const std::vector<ObservedLogLine>& lines)
{
    if (_updates_paused)
    {
        _paused_updates.insert(_paused_updates.end(), lines.begin(), lines.end());
    }
    else
    {
        append_lines_immediately(lines);
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

void LogViewModel::append_lines_immediately(const std::vector<ObservedLogLine>& lines)
{
    _entries.insert(_entries.end(), lines.begin(), lines.end());
    if (_follow_bottom)
    {
        _scroll_offset = max_scroll_offset();
    }
}

void LogViewModel::flush_paused_updates()
{
    append_lines_immediately(_paused_updates);
    _paused_updates.clear();
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
