#include "log_controller.hpp"

#include <algorithm>
#include <sstream>

namespace slayerlog
{

namespace
{

bool is_before(const TextPosition& lhs, const TextPosition& rhs)
{
    return lhs.line < rhs.line || (lhs.line == rhs.line && lhs.column < rhs.column);
}

} // namespace

void LogController::reset()
{
    _first_visible_line_index = VisibleLineIndex {0};
    _follow_bottom            = true;

    _active_find_entry_index.reset();

    _selection_in_progress = false;
    _selection_anchor.reset();
    _selection_focus.reset();
}

VisibleLineIndex LogController::first_visible_line_index(const LogModel& model, int viewport_line_count) const
{
    if (_follow_bottom)
    {
        return VisibleLineIndex {max_first_visible_line_index(model, viewport_line_count)};
    }

    return VisibleLineIndex {std::clamp(_first_visible_line_index.value, 0, max_first_visible_line_index(model, viewport_line_count))};
}

void LogController::scroll_up(const LogModel& model, int viewport_line_count, int amount)
{
    _first_visible_line_index =
        VisibleLineIndex {std::max(0, first_visible_line_index(model, viewport_line_count).value - std::max(1, amount))};
    _follow_bottom = _first_visible_line_index.value >= max_first_visible_line_index(model, viewport_line_count);
}

void LogController::scroll_down(const LogModel& model, int viewport_line_count, int amount)
{
    _first_visible_line_index = VisibleLineIndex {std::min(first_visible_line_index(model, viewport_line_count).value + std::max(1, amount),
                                                           max_first_visible_line_index(model, viewport_line_count))};
    _follow_bottom            = _first_visible_line_index.value >= max_first_visible_line_index(model, viewport_line_count);
}

void LogController::scroll_to_top(const LogModel& model, int viewport_line_count)
{
    _first_visible_line_index = VisibleLineIndex {0};
    _follow_bottom            = _first_visible_line_index.value >= max_first_visible_line_index(model, viewport_line_count);
}

void LogController::scroll_to_bottom()
{
    _follow_bottom = true;
}

bool LogController::go_to_line(const LogModel& model, int line_number, int viewport_line_count)
{
    const auto target_visible_index = model.visible_line_index_for_line_number(line_number);
    if (!target_visible_index.has_value())
    {
        return false;
    }

    center_on_visible_line(model, *target_visible_index, viewport_line_count);
    return true;
}

bool LogController::set_find_query(LogModel& model, std::string query, int viewport_line_count)
{
    const bool has_matches = model.set_find_query(std::move(query));
    _active_find_entry_index.reset();
    if (!has_matches)
    {
        return false;
    }

    return go_to_next_find_match(model, viewport_line_count);
}

void LogController::clear_find(LogModel& model)
{
    model.clear_find_query();
    _active_find_entry_index.reset();
}

bool LogController::go_to_next_find_match(const LogModel& model, int viewport_line_count)
{
    if (!model.find_active() || model.total_find_match_count() == 0)
    {
        return false;
    }

    int current_position = -1;
    if (_active_find_entry_index.has_value())
    {
        const auto position = model.find_match_position_for_entry_index(*_active_find_entry_index);
        if (position.has_value())
        {
            current_position = position->value;
        }
    }

    for (int offset = 1; offset <= model.total_find_match_count(); ++offset)
    {
        const int next_position = (current_position + offset) % model.total_find_match_count();
        const auto entry_index  = model.find_match_entry_index(FindResultIndex {next_position});
        if (!entry_index.has_value() || !model.entry_index_is_visible(*entry_index))
        {
            continue;
        }

        _active_find_entry_index = *entry_index;
        const auto visible_index = model.visible_line_index_for_entry(*entry_index);
        if (!visible_index.has_value())
        {
            return false;
        }

        center_on_visible_line(model, *visible_index, viewport_line_count);
        return true;
    }

    return false;
}

bool LogController::go_to_previous_find_match(const LogModel& model, int viewport_line_count)
{
    if (!model.find_active() || model.total_find_match_count() == 0)
    {
        return false;
    }

    int current_position = 0;
    if (_active_find_entry_index.has_value())
    {
        const auto position = model.find_match_position_for_entry_index(*_active_find_entry_index);
        if (position.has_value())
        {
            current_position = position->value;
        }
    }

    for (int offset = 1; offset <= model.total_find_match_count(); ++offset)
    {
        const int previous_position = (current_position - offset + model.total_find_match_count()) % model.total_find_match_count();
        const auto entry_index      = model.find_match_entry_index(FindResultIndex {previous_position});
        if (!entry_index.has_value() || !model.entry_index_is_visible(*entry_index))
        {
            continue;
        }

        _active_find_entry_index = *entry_index;
        const auto visible_index = model.visible_line_index_for_entry(*entry_index);
        if (!visible_index.has_value())
        {
            return false;
        }

        center_on_visible_line(model, *visible_index, viewport_line_count);
        return true;
    }

    return false;
}

std::optional<VisibleLineIndex> LogController::active_find_visible_index(const LogModel& model) const
{
    if (!_active_find_entry_index.has_value())
    {
        return std::nullopt;
    }

    return model.visible_line_index_for_entry(*_active_find_entry_index);
}

void LogController::begin_selection(const LogModel& model, TextPosition position)
{
    _selection_anchor      = clamp_selection_position(model, position);
    _selection_focus       = _selection_anchor;
    _selection_in_progress = _selection_anchor.has_value();
}

void LogController::update_selection(const LogModel& model, TextPosition position)
{
    if (!_selection_in_progress || !_selection_anchor.has_value())
    {
        return;
    }

    _selection_focus = clamp_selection_position(model, position);
}

void LogController::end_selection(const LogModel& model, std::optional<TextPosition> position)
{
    _selection_in_progress = false;
    if (position.has_value() && _selection_anchor.has_value())
    {
        _selection_focus = clamp_selection_position(model, *position);
    }
}

void LogController::clear_selection()
{
    _selection_anchor.reset();
    _selection_focus.reset();
    _selection_in_progress = false;
}

bool LogController::selection_in_progress() const
{
    return _selection_in_progress;
}

std::optional<std::pair<TextPosition, TextPosition>> LogController::selection_bounds(const LogModel& model) const
{
    if (!_selection_anchor.has_value() || !_selection_focus.has_value() || model.line_count() == 0)
    {
        return std::nullopt;
    }

    auto start = clamp_selection_position(model, *_selection_anchor);
    auto end   = clamp_selection_position(model, *_selection_focus);
    if (is_before(end, start))
    {
        std::swap(start, end);
    }

    return std::pair(start, end);
}

std::string LogController::selection_text(const LogModel& model) const
{
    const auto bounds = selection_bounds(model);
    if (!bounds.has_value())
    {
        return {};
    }

    const auto [start, end] = *bounds;
    std::ostringstream output;
    for (int line_index = start.line; line_index <= end.line; ++line_index)
    {
        const auto line            = model.rendered_line(line_index);
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

int LogController::max_first_visible_line_index(const LogModel& model, int viewport_line_count) const
{
    return std::max(0, model.line_count() - std::max(1, viewport_line_count));
}

void LogController::center_on_visible_line(const LogModel& model, VisibleLineIndex target_visible_index, int viewport_line_count)
{
    _first_visible_line_index = VisibleLineIndex {target_visible_index.value - (std::max(1, viewport_line_count) / 2)};
    _first_visible_line_index.value =
        std::clamp(_first_visible_line_index.value, 0, max_first_visible_line_index(model, viewport_line_count));
    _follow_bottom = _first_visible_line_index.value >= max_first_visible_line_index(model, viewport_line_count);
}

TextPosition LogController::clamp_selection_position(const LogModel& model, TextPosition position) const
{
    if (model.line_count() == 0)
    {
        return TextPosition {0, 0};
    }

    position.line          = std::clamp(position.line, 0, model.line_count() - 1);
    const auto line_length = static_cast<int>(model.rendered_line(position.line).size());
    position.column        = std::clamp(position.column, 0, line_length);
    return position;
}

} // namespace slayerlog
