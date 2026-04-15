#include "log_controller.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace slayerlog
{

LogController::LogController() : _text_view_controller(_text_view_model)
{
}

void LogController::reset()
{
    _buffer_a.clear();
    _buffer_b.clear();
    _active_buffer_is_a = true;
    _synced_line_count  = 0;
    _active_find_entry_index.reset();
    _text_view_controller.swap_lines(active_buffer());
    _text_view_controller.scroll_to_bottom();
}

// --- Content management ---

void LogController::rebuild_view(const LogModel& model)
{
    auto& target = inactive_buffer();
    target.clear();

    const int count = model.line_count();
    target.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        target.push_back(model.rendered_line(i));
    }

    _active_buffer_is_a = !_active_buffer_is_a;
    _synced_line_count  = count;
    _text_view_controller.swap_lines(active_buffer());
}

void LogController::sync_view(const LogModel& model)
{
    const int current_count = model.line_count();
    if (current_count < _synced_line_count)
    {
        // Line count decreased (shouldn't happen for streaming, but handle safely)
        rebuild_view(model);
        return;
    }

    if (current_count == _synced_line_count)
    {
        return;
    }

    auto& buffer = active_buffer();
    buffer.reserve(static_cast<std::size_t>(current_count));
    for (int i = _synced_line_count; i < current_count; ++i)
    {
        buffer.push_back(model.rendered_line(i));
    }
    _synced_line_count = current_count;
    _text_view_controller.notify_lines_appended();
}

// --- Domain-specific navigation ---

bool LogController::go_to_line(const LogModel& model, int line_number)
{
    const auto target_visible_index = model.visible_line_index_for_line_number(line_number);
    if (!target_visible_index.has_value())
    {
        return false;
    }

    _text_view_controller.center_on_line(target_visible_index->value);
    return true;
}

// --- Find ---

bool LogController::set_find_query(LogModel& model, std::string query)
{
    const bool has_matches = model.set_find_query(std::move(query));
    _active_find_entry_index.reset();
    if (!has_matches)
    {
        return false;
    }

    return go_to_next_find_match(model);
}

void LogController::clear_find(LogModel& model)
{
    model.clear_find_query();
    _active_find_entry_index.reset();
}

bool LogController::go_to_next_find_match(const LogModel& model)
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

        _text_view_controller.center_on_line(visible_index->value);
        return true;
    }

    return false;
}

bool LogController::go_to_previous_find_match(const LogModel& model)
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

        _text_view_controller.center_on_line(visible_index->value);
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

// --- Event handling ---

LogEventResult LogController::handle_event(LogModel& model, ftxui::Event event, const std::function<std::optional<TextViewPosition>(const ftxui::Mouse&)>& mouse_to_text_position)
{
    // Escape: clear find if active, otherwise delegate (TextViewController handles exit)
    if (event == ftxui::Event::Escape && model.find_active())
    {
        clear_find(model);
        return {true, false};
    }

    // Custom event (re-render trigger)
    if (event == ftxui::Event::Custom)
    {
        return {true, false};
    }

    // Pause toggle
    if (event == ftxui::Event::Character('p'))
    {
        model.toggle_pause();
        if (!model.updates_paused())
        {
            // Unpausing flushes buffered updates
            sync_view(model);
        }
        return {true, false};
    }

    // Find navigation: intercept arrow keys when find is active
    if (model.find_active() && event == ftxui::Event::ArrowRight)
    {
        return {go_to_next_find_match(model), false};
    }

    if (model.find_active() && event == ftxui::Event::ArrowLeft)
    {
        return {go_to_previous_find_match(model), false};
    }

    // Delegate everything else to the generic text view controller
    auto result = _text_view_controller.parse_event(event, mouse_to_text_position);
    return {result.handled, result.request_exit};
}

// --- Access to underlying text view ---

TextViewController& LogController::text_view_controller()
{
    return _text_view_controller;
}

const TextViewController& LogController::text_view_controller() const
{
    return _text_view_controller;
}

const TextViewModel& LogController::text_view_model() const
{
    return _text_view_model;
}

// --- Private ---

std::vector<std::string>& LogController::active_buffer()
{
    return _active_buffer_is_a ? _buffer_a : _buffer_b;
}

std::vector<std::string>& LogController::inactive_buffer()
{
    return _active_buffer_is_a ? _buffer_b : _buffer_a;
}

} // namespace slayerlog
