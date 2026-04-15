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

void LogController::rebuild_view(const ProcessedSources& processed_sources)
{
    auto& target = inactive_buffer();
    target.clear();

    const int count = processed_sources.line_count();
    target.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        target.push_back(processed_sources.rendered_line(i));
    }

    _active_buffer_is_a = !_active_buffer_is_a;
    _synced_line_count  = count;
    _text_view_controller.swap_lines(active_buffer());
}

void LogController::sync_view(const ProcessedSources& processed_sources)
{
    const int current_count = processed_sources.line_count();
    if (current_count < _synced_line_count)
    {
        // Line count decreased (shouldn't happen for streaming, but handle safely)
        rebuild_view(processed_sources);
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
        buffer.push_back(processed_sources.rendered_line(i));
    }
    _synced_line_count = current_count;
    _text_view_controller.notify_lines_appended();
}

// --- Domain-specific navigation ---

bool LogController::go_to_line(const ProcessedSources& processed_sources, int line_number)
{
    const auto target_visible_index = processed_sources.visible_line_index_for_line_number(line_number);
    if (!target_visible_index.has_value())
    {
        return false;
    }

    _text_view_controller.center_on_line(target_visible_index->value);
    return true;
}

// --- Find ---

bool LogController::set_find_query(ProcessedSources& processed_sources, std::string query)
{
    const bool has_matches = processed_sources.set_find_query(std::move(query));
    _active_find_entry_index.reset();
    if (!has_matches)
    {
        return false;
    }

    return go_to_next_find_match(processed_sources);
}

void LogController::clear_find(ProcessedSources& processed_sources)
{
    processed_sources.clear_find_query();
    _active_find_entry_index.reset();
}

bool LogController::go_to_next_find_match(const ProcessedSources& processed_sources)
{
    if (!processed_sources.find_active() || processed_sources.total_find_match_count() == 0)
    {
        return false;
    }

    int current_position = -1;
    if (_active_find_entry_index.has_value())
    {
        const auto position = processed_sources.find_match_position_for_entry_index(*_active_find_entry_index);
        if (position.has_value())
        {
            current_position = position->value;
        }
    }

    for (int offset = 1; offset <= processed_sources.total_find_match_count(); ++offset)
    {
        const int next_position = (current_position + offset) % processed_sources.total_find_match_count();
        const auto entry_index  = processed_sources.find_match_entry_index(FindResultIndex {next_position});
        if (!entry_index.has_value() || !processed_sources.entry_index_is_visible(*entry_index))
        {
            continue;
        }

        _active_find_entry_index = *entry_index;
        const auto visible_index = processed_sources.visible_line_index_for_entry(*entry_index);
        if (!visible_index.has_value())
        {
            return false;
        }

        _text_view_controller.center_on_line(visible_index->value);
        return true;
    }

    return false;
}

bool LogController::go_to_previous_find_match(const ProcessedSources& processed_sources)
{
    if (!processed_sources.find_active() || processed_sources.total_find_match_count() == 0)
    {
        return false;
    }

    int current_position = 0;
    if (_active_find_entry_index.has_value())
    {
        const auto position = processed_sources.find_match_position_for_entry_index(*_active_find_entry_index);
        if (position.has_value())
        {
            current_position = position->value;
        }
    }

    for (int offset = 1; offset <= processed_sources.total_find_match_count(); ++offset)
    {
        const int previous_position = (current_position - offset + processed_sources.total_find_match_count()) % processed_sources.total_find_match_count();
        const auto entry_index      = processed_sources.find_match_entry_index(FindResultIndex {previous_position});
        if (!entry_index.has_value() || !processed_sources.entry_index_is_visible(*entry_index))
        {
            continue;
        }

        _active_find_entry_index = *entry_index;
        const auto visible_index = processed_sources.visible_line_index_for_entry(*entry_index);
        if (!visible_index.has_value())
        {
            return false;
        }

        _text_view_controller.center_on_line(visible_index->value);
        return true;
    }

    return false;
}

std::optional<VisibleLineIndex> LogController::active_find_visible_index(const ProcessedSources& processed_sources) const
{
    if (!_active_find_entry_index.has_value())
    {
        return std::nullopt;
    }

    return processed_sources.visible_line_index_for_entry(*_active_find_entry_index);
}

// --- Event handling ---

LogEventResult LogController::handle_event(ProcessedSources& processed_sources, ftxui::Event event, const std::function<std::optional<TextViewPosition>(const ftxui::Mouse&)>& mouse_to_text_position)
{
    // Escape: clear find if active, otherwise delegate (TextViewController handles exit)
    if (event == ftxui::Event::Escape && processed_sources.find_active())
    {
        clear_find(processed_sources);
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
        processed_sources.toggle_pause();
        if (!processed_sources.updates_paused())
        {
            // Unpausing flushes buffered updates
            sync_view(processed_sources);
        }
        return {true, false};
    }

    // Find navigation: intercept arrow keys when find is active
    if (processed_sources.find_active() && event == ftxui::Event::ArrowRight)
    {
        return {go_to_next_find_match(processed_sources), false};
    }

    if (processed_sources.find_active() && event == ftxui::Event::ArrowLeft)
    {
        return {go_to_previous_find_match(processed_sources), false};
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
