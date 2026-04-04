#include "log_view_model.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iterator>
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

void LogViewModel::add_include_filter(std::string filter_text)
{
    const bool was_following_bottom = _follow_bottom;
    filter_text                     = trim_filter_text(filter_text);
    if (filter_text.empty())
    {
        return;
    }

    _include_filters.push_back(std::move(filter_text));
    rebuild_visible_entries();
    clamp_selection();
    if (was_following_bottom)
    {
        _scroll_offset = max_scroll_offset();
    }
    else
    {
        clamp_scroll_offset();
    }
    update_follow_bottom();
}

void LogViewModel::add_exclude_filter(std::string filter_text)
{
    const bool was_following_bottom = _follow_bottom;
    filter_text                     = trim_filter_text(filter_text);
    if (filter_text.empty())
    {
        return;
    }

    _exclude_filters.push_back(std::move(filter_text));
    rebuild_visible_entries();
    clamp_selection();
    if (was_following_bottom)
    {
        _scroll_offset = max_scroll_offset();
    }
    else
    {
        clamp_scroll_offset();
    }
    update_follow_bottom();
}

void LogViewModel::reset_filters()
{
    const bool was_following_bottom = _follow_bottom;
    _include_filters.clear();
    _exclude_filters.clear();
    rebuild_visible_entries();
    clamp_selection();
    if (was_following_bottom)
    {
        _scroll_offset = max_scroll_offset();
    }
    else
    {
        clamp_scroll_offset();
    }
    update_follow_bottom();
}

const std::vector<std::string>& LogViewModel::include_filters() const
{
    return _include_filters;
}

const std::vector<std::string>& LogViewModel::exclude_filters() const
{
    return _exclude_filters;
}

void LogViewModel::hide_before_line_number(int line_number)
{
    const bool was_following_bottom = _follow_bottom;
    _hidden_before_line_number      = line_number > 1 ? std::optional<int>(line_number) : std::nullopt;
    rebuild_visible_entries();
    clamp_selection();
    if (was_following_bottom)
    {
        _scroll_offset = max_scroll_offset();
    }
    else
    {
        clamp_scroll_offset();
    }
    update_follow_bottom();
}

std::optional<int> LogViewModel::hidden_before_line_number() const
{
    return _hidden_before_line_number;
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
    return static_cast<int>(_visible_entry_indices.size());
}

int LogViewModel::total_line_count() const
{
    return static_cast<int>(_all_entries.size());
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

bool LogViewModel::center_on_line_number(int line_number)
{
    if (line_number <= 0)
    {
        return false;
    }

    const int target_entry_index    = line_number - 1;
    const auto target_visible_entry = std::find(_visible_entry_indices.begin(), _visible_entry_indices.end(), target_entry_index);
    if (target_visible_entry == _visible_entry_indices.end())
    {
        return false;
    }

    const int target_visible_index = static_cast<int>(std::distance(_visible_entry_indices.begin(), target_visible_entry));
    _scroll_offset                 = target_visible_index - (_visible_line_count / 2);
    clamp_scroll_offset();
    update_follow_bottom();
    return true;
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
    if (!bounds.has_value() || _visible_entry_indices.empty())
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
    const int visible_index = _visible_entry_indices[static_cast<std::size_t>(index)];
    const auto& entry       = _all_entries[static_cast<std::size_t>(visible_index)];
    output << visible_index + 1 << " ";
    if (_show_source_labels)
    {
        output << "[" << entry.source_label << "] ";
    }

    output << entry.text;
    return output.str();
}

void LogViewModel::append_lines_immediately(const std::vector<ObservedLogLine>& lines)
{
    for (const auto& line : lines)
    {
        _all_entries.push_back(line);
        if (line_number_is_visible(static_cast<int>(_all_entries.size())) && entry_matches_filters(line))
        {
            _visible_entry_indices.push_back(static_cast<int>(_all_entries.size()) - 1);
        }
    }

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

void LogViewModel::rebuild_visible_entries()
{
    _visible_entry_indices.clear();
    _visible_entry_indices.reserve(_all_entries.size());

    for (std::size_t index = 0; index < _all_entries.size(); ++index)
    {
        if (line_number_is_visible(static_cast<int>(index) + 1) && entry_matches_filters(_all_entries[index]))
        {
            _visible_entry_indices.push_back(static_cast<int>(index));
        }
    }
}

void LogViewModel::clamp_scroll_offset()
{
    _scroll_offset = std::clamp(_scroll_offset, 0, max_scroll_offset());
}

int LogViewModel::max_scroll_offset() const
{
    return std::max(0, static_cast<int>(_visible_entry_indices.size()) - _visible_line_count);
}

void LogViewModel::update_follow_bottom()
{
    _follow_bottom = _scroll_offset >= max_scroll_offset();
}

void LogViewModel::clamp_selection()
{
    if (_visible_entry_indices.empty())
    {
        clear_selection();
        return;
    }

    auto clamp_position = [&](TextPosition& position)
    {
        position.line          = std::clamp(position.line, 0, static_cast<int>(_visible_entry_indices.size()) - 1);
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

bool LogViewModel::entry_matches_filters(const ObservedLogLine& entry) const
{
    const std::string searchable_text = entry.source_label + "\n" + entry.text;
    const bool matches_include        = _include_filters.empty() || matches_any_filter(searchable_text, _include_filters);
    const bool matches_exclude        = matches_any_filter(searchable_text, _exclude_filters);
    return matches_include && !matches_exclude;
}

bool LogViewModel::line_number_is_visible(int line_number) const
{
    return !_hidden_before_line_number.has_value() || line_number >= *_hidden_before_line_number;
}

bool LogViewModel::matches_any_filter(std::string_view haystack, const std::vector<std::string>& filters) const
{
    return std::any_of(filters.begin(), filters.end(),
                       [&](const std::string& filter) { return haystack.find(filter) != std::string_view::npos; });
}

std::string LogViewModel::trim_filter_text(std::string_view text)
{
    std::size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0)
    {
        ++start;
    }

    std::size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
    {
        --end;
    }

    return std::string(text.substr(start, end - start));
}

} // namespace slayerlog
