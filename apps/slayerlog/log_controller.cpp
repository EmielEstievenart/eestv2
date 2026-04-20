#include "log_controller.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>

namespace slayerlog
{

LogController::LogController() = default;

void LogController::reset()
{
    _buffer_a.clear();
    _buffer_b.clear();
    _active_buffer_is_a = true;
    _synced_line_count  = 0;
    _max_line_width     = 0;
    _find_query.clear();
    _find_pattern.reset();
    _find_match_entry_indices.clear();
    _active_find_entry_index.reset();
    _sync_selection_active = false;
    _sync_selecting_destination = false;
    _sync_target_visible_index.reset();
    _sync_source_entry_index.reset();
    _sync_apply_handler = {};
    _status_message.clear();
    _status_is_error = false;
    _text_view_controller.set_content(0, 0, [this](int index) -> const std::string& { return active_buffer()[static_cast<std::size_t>(index)]; });
    _text_view_controller.scroll_to_bottom();
}

// --- Content management ---

void LogController::rebuild_view(const AllProcessedSources& processed_sources)
{
    auto& target = inactive_buffer();
    target.clear();

    const int count = processed_sources.line_count();
    target.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        target.push_back(processed_sources.rendered_line(i));
    }

    rebuild_find_matches(processed_sources);
    _active_buffer_is_a = !_active_buffer_is_a;
    _synced_line_count  = count;

    _max_line_width = 0;
    for (const auto& line : active_buffer())
    {
        _max_line_width = std::max(_max_line_width, static_cast<int>(line.size()));
    }

    _text_view_controller.set_content(count, _max_line_width, [this](int index) -> const std::string& { return active_buffer()[static_cast<std::size_t>(index)]; });
}

void LogController::sync_view(const AllProcessedSources& processed_sources)
{
    const int current_count = processed_sources.line_count();
    if (current_count < _synced_line_count)
    {
        // Line count decreased (shouldn't happen for streaming, but handle safely)
        rebuild_view(processed_sources);
        return;
    }

    if (_synced_line_count > 0 && static_cast<int>(active_buffer().size()) >= _synced_line_count && active_buffer().back() != processed_sources.rendered_line(_synced_line_count - 1))
    {
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
        _max_line_width = std::max(_max_line_width, static_cast<int>(buffer.back().size()));
    }

    expand_find_matches(processed_sources, AllLineIndex {_synced_line_count});
    _synced_line_count = current_count;
    _text_view_controller.update_content_size(current_count, _max_line_width);
}

// --- Domain-specific navigation ---

bool LogController::go_to_line(const AllProcessedSources& processed_sources, int line_number)
{
    const auto target_visible_index = processed_sources.visible_line_index_for_line_number(line_number);
    if (!target_visible_index.has_value())
    {
        return false;
    }

    _text_view_controller.center_on_line(target_visible_index->value);
    return true;
}

bool LogController::start_sync_selection(const AllProcessedSources& processed_sources, SyncApplyHandler apply_handler, std::string& message)
{
    const auto first_selectable = first_sync_selectable_visible_index(processed_sources);
    if (!first_selectable.has_value())
    {
        message = "No visible timestamped lines are available for source synchronisation";
        return false;
    }

    _sync_selection_active = true;
    _sync_selecting_destination = false;
    _sync_target_visible_index = first_selectable;
    _sync_source_entry_index.reset();
    _sync_apply_handler = std::move(apply_handler);
    ensure_sync_target_visible();
    set_status_message("Synchronise mode active", false);
    message = "Select the source line with Up/Down and Enter";
    return true;
}

bool LogController::sync_selection_active() const
{
    return _sync_selection_active;
}

std::optional<VisibleLineIndex> LogController::sync_target_visible_index() const
{
    return _sync_target_visible_index;
}

std::optional<VisibleLineIndex> LogController::sync_source_visible_index(const AllProcessedSources& processed_sources) const
{
    if (!_sync_source_entry_index.has_value())
    {
        return std::nullopt;
    }

    return processed_sources.visible_line_index_for_entry(*_sync_source_entry_index);
}

std::string LogController::sync_selection_status(const AllProcessedSources& processed_sources) const
{
    if (!_sync_selection_active || !_sync_target_visible_index.has_value())
    {
        return {};
    }

    std::ostringstream output;
    output << (_sync_selecting_destination ? "SYNC destination" : "SYNC source") << " line";

    const auto current_entry = sync_entry_at_visible_index(processed_sources, *_sync_target_visible_index);
    if (current_entry != nullptr)
    {
        output << " | current " << current_entry->metadata.source_label << " -> line " << (_sync_target_visible_index->value + 1);
    }

    if (_sync_source_entry_index.has_value())
    {
        const auto& source_entry = processed_sources.entry_at(*_sync_source_entry_index);
        output << " | source " << source_entry.metadata.source_label;
    }

    return output.str();
}

const std::string& LogController::status_message() const
{
    return _status_message;
}

bool LogController::status_is_error() const
{
    return _status_is_error;
}

void LogController::clear_status_message()
{
    _status_message.clear();
    _status_is_error = false;
}

// --- Find ---

bool LogController::set_find_query(AllProcessedSources& processed_sources, std::string query)
{
    query = trim_search_text(query);
    if (query.empty())
    {
        clear_find(processed_sources);
        return false;
    }

    const SearchPattern pattern = compile_search_pattern(query);

    _find_query   = pattern.raw_text;
    _find_pattern = pattern;
    rebuild_find_matches(processed_sources);
    _active_find_entry_index.reset();
    const bool has_matches = !_find_match_entry_indices.empty();
    if (!has_matches)
    {
        return false;
    }

    return go_to_next_find_match(processed_sources);
}

void LogController::clear_find(AllProcessedSources& processed_sources)
{
    (void)processed_sources;
    _find_query.clear();
    _find_pattern.reset();
    _find_match_entry_indices.clear();
    _active_find_entry_index.reset();
}

bool LogController::find_active() const
{
    return !_find_query.empty();
}

const std::string& LogController::find_query() const
{
    return _find_query;
}

int LogController::total_find_match_count() const
{
    return static_cast<int>(_find_match_entry_indices.size());
}

int LogController::visible_find_match_count(const AllProcessedSources& processed_sources) const
{
    return static_cast<int>(std::count_if(_find_match_entry_indices.begin(), _find_match_entry_indices.end(), [&](AllLineIndex entry_index) { return processed_sources.entry_index_is_visible(entry_index); }));
}

bool LogController::visible_line_matches_find(const AllProcessedSources& processed_sources, int visible_index) const
{
    if (visible_index < 0)
    {
        return false;
    }

    const auto entry_index = processed_sources.entry_index_for_visible_line(VisibleLineIndex {visible_index});
    if (!entry_index.has_value())
    {
        return false;
    }

    return std::binary_search(_find_match_entry_indices.begin(), _find_match_entry_indices.end(), *entry_index);
}

bool LogController::go_to_next_find_match(const AllProcessedSources& processed_sources)
{
    if (!find_active() || total_find_match_count() == 0)
    {
        return false;
    }

    int current_position = -1;
    if (_active_find_entry_index.has_value())
    {
        const auto position = std::find(_find_match_entry_indices.begin(), _find_match_entry_indices.end(), *_active_find_entry_index);
        if (position != _find_match_entry_indices.end())
        {
            current_position = static_cast<int>(std::distance(_find_match_entry_indices.begin(), position));
        }
    }

    for (int offset = 1; offset <= total_find_match_count(); ++offset)
    {
        const int next_position        = (current_position + offset) % total_find_match_count();
        const AllLineIndex entry_index = _find_match_entry_indices[FindResultIndex {next_position}];
        if (!processed_sources.entry_index_is_visible(entry_index))
        {
            continue;
        }

        _active_find_entry_index = entry_index;
        const auto visible_index = processed_sources.visible_line_index_for_entry(entry_index);
        if (!visible_index.has_value())
        {
            return false;
        }

        _text_view_controller.center_on_line(visible_index->value);
        return true;
    }

    return false;
}

bool LogController::go_to_previous_find_match(const AllProcessedSources& processed_sources)
{
    if (!find_active() || total_find_match_count() == 0)
    {
        return false;
    }

    int current_position = 0;
    if (_active_find_entry_index.has_value())
    {
        const auto position = std::find(_find_match_entry_indices.begin(), _find_match_entry_indices.end(), *_active_find_entry_index);
        if (position != _find_match_entry_indices.end())
        {
            current_position = static_cast<int>(std::distance(_find_match_entry_indices.begin(), position));
        }
    }

    for (int offset = 1; offset <= total_find_match_count(); ++offset)
    {
        const int previous_position    = (current_position - offset + total_find_match_count()) % total_find_match_count();
        const AllLineIndex entry_index = _find_match_entry_indices[FindResultIndex {previous_position}];
        if (!processed_sources.entry_index_is_visible(entry_index))
        {
            continue;
        }

        _active_find_entry_index = entry_index;
        const auto visible_index = processed_sources.visible_line_index_for_entry(entry_index);
        if (!visible_index.has_value())
        {
            return false;
        }

        _text_view_controller.center_on_line(visible_index->value);
        return true;
    }

    return false;
}

std::optional<VisibleLineIndex> LogController::active_find_visible_index(const AllProcessedSources& processed_sources) const
{
    if (!_active_find_entry_index.has_value())
    {
        return std::nullopt;
    }

    return processed_sources.visible_line_index_for_entry(*_active_find_entry_index);
}

// --- Event handling ---

LogEventResult LogController::handle_event(AllProcessedSources& processed_sources, ftxui::Event event, const std::function<std::optional<TextViewPosition>(const ftxui::Mouse&)>& mouse_to_text_position)
{
    if (_sync_selection_active)
    {
        if (event == ftxui::Event::Escape)
        {
            stop_sync_selection();
            set_status_message("Synchronise mode cancelled", false);
            return {true, false};
        }

        if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k'))
        {
            const auto next_visible = next_sync_selectable_visible_index(processed_sources, _sync_target_visible_index->value - 1, -1);
            if (next_visible.has_value())
            {
                _sync_target_visible_index = next_visible;
                ensure_sync_target_visible();
            }
            return {true, false};
        }

        if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j'))
        {
            const auto next_visible = next_sync_selectable_visible_index(processed_sources, _sync_target_visible_index->value + 1, 1);
            if (next_visible.has_value())
            {
                _sync_target_visible_index = next_visible;
                ensure_sync_target_visible();
            }
            return {true, false};
        }

        if (event == ftxui::Event::Home)
        {
            const auto first_visible = first_sync_selectable_visible_index(processed_sources);
            if (first_visible.has_value())
            {
                _sync_target_visible_index = first_visible;
                ensure_sync_target_visible();
            }
            return {true, false};
        }

        if (event == ftxui::Event::End)
        {
            const auto last_visible = next_sync_selectable_visible_index(processed_sources, processed_sources.line_count() - 1, -1);
            if (last_visible.has_value())
            {
                _sync_target_visible_index = last_visible;
                ensure_sync_target_visible();
            }
            return {true, false};
        }

        if (event == ftxui::Event::Return)
        {
            const LogEntry* current_entry = sync_entry_at_visible_index(processed_sources, *_sync_target_visible_index);
            if (current_entry == nullptr)
            {
                set_status_message("Select a visible line with a parsed timestamp", true);
                return {true, false};
            }

            if (!_sync_selecting_destination)
            {
                _sync_source_entry_index = processed_sources.entry_index_for_visible_line(*_sync_target_visible_index);
                _sync_selecting_destination = true;
                set_status_message("Source selected; now select the destination line", false);

                const auto next_visible = next_sync_selectable_visible_index(processed_sources, _sync_target_visible_index->value + 1, 1);
                if (next_visible.has_value())
                {
                    _sync_target_visible_index = next_visible;
                    ensure_sync_target_visible();
                }

                return {true, false};
            }

            if (!_sync_source_entry_index.has_value() || !_sync_apply_handler)
            {
                stop_sync_selection();
                set_status_message("Synchronise mode state was lost", true);
                return {true, false};
            }

            const auto& source_entry = processed_sources.entry_at(*_sync_source_entry_index);
            const SyncApplyResult result = _sync_apply_handler(source_entry, *current_entry);
            if (result.success)
            {
                stop_sync_selection();
                set_status_message(result.message, false);
            }
            else
            {
                set_status_message(result.message, true);
            }

            return {true, false};
        }

        return {false, false};
    }

    // Escape: clear find if active, otherwise delegate (TextViewController handles exit)
    if (event == ftxui::Event::Escape && find_active())
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
            rebuild_view(processed_sources);
            (void)processed_sources.consume_column_width_growth();
        }
        return {true, false};
    }

    // Find navigation: intercept arrow keys when find is active
    if (find_active() && event == ftxui::Event::ArrowRight)
    {
        return {go_to_next_find_match(processed_sources), false};
    }

    if (find_active() && event == ftxui::Event::ArrowLeft)
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

const std::string& LogController::line_at(int index) const
{
    return active_buffer().at(static_cast<std::size_t>(index));
}

// --- Private ---

std::vector<std::string>& LogController::active_buffer()
{
    return _active_buffer_is_a ? _buffer_a : _buffer_b;
}

const std::vector<std::string>& LogController::active_buffer() const
{
    return _active_buffer_is_a ? _buffer_a : _buffer_b;
}

std::vector<std::string>& LogController::inactive_buffer()
{
    return _active_buffer_is_a ? _buffer_b : _buffer_a;
}

void LogController::rebuild_find_matches(const AllProcessedSources& processed_sources)
{
    _find_match_entry_indices.clear();
    if (!find_active())
    {
        return;
    }

    _find_match_entry_indices.reserve(static_cast<std::size_t>(processed_sources.total_line_count()));
    for (int index = 0; index < processed_sources.total_line_count(); ++index)
    {
        const AllLineIndex entry_index {index};
        if (entry_matches_find_query(processed_sources.entry_at(entry_index)))
        {
            _find_match_entry_indices.push_back(entry_index);
        }
    }
}

void LogController::expand_find_matches(const AllProcessedSources& processed_sources, AllLineIndex first_new_entry_index)
{
    if (!find_active())
    {
        return;
    }

    for (int index = first_new_entry_index.value; index < processed_sources.total_line_count(); ++index)
    {
        const AllLineIndex entry_index {index};
        if (entry_matches_find_query(processed_sources.entry_at(entry_index)))
        {
            _find_match_entry_indices.push_back(entry_index);
        }
    }
}

bool LogController::entry_matches_find_query(const LogEntry& entry) const
{
    return _find_pattern.has_value() && matches_pattern(entry.text, *_find_pattern);
}

std::optional<VisibleLineIndex> LogController::first_sync_selectable_visible_index(const AllProcessedSources& processed_sources) const
{
    return next_sync_selectable_visible_index(processed_sources, 0, 1);
}

std::optional<VisibleLineIndex> LogController::next_sync_selectable_visible_index(const AllProcessedSources& processed_sources, int start_index, int step) const
{
    if (processed_sources.line_count() == 0 || step == 0)
    {
        return std::nullopt;
    }

    for (int visible_index = start_index; visible_index >= 0 && visible_index < processed_sources.line_count(); visible_index += step)
    {
        const VisibleLineIndex candidate {visible_index};
        if (is_sync_selectable_visible_index(processed_sources, candidate))
        {
            return candidate;
        }
    }

    return std::nullopt;
}

bool LogController::is_sync_selectable_visible_index(const AllProcessedSources& processed_sources, VisibleLineIndex visible_index) const
{
    const auto entry_index = processed_sources.entry_index_for_visible_line(visible_index);
    if (!entry_index.has_value())
    {
        return false;
    }

    const auto& entry = processed_sources.entry_at(*entry_index);
    return entry.metadata.timestamp.has_value() && entry.metadata.source != nullptr;
}

const LogEntry* LogController::sync_entry_at_visible_index(const AllProcessedSources& processed_sources, VisibleLineIndex visible_index) const
{
    if (!is_sync_selectable_visible_index(processed_sources, visible_index))
    {
        return nullptr;
    }

    const auto entry_index = processed_sources.entry_index_for_visible_line(visible_index);
    if (!entry_index.has_value())
    {
        return nullptr;
    }

    return &processed_sources.entry_at(*entry_index);
}

void LogController::ensure_sync_target_visible()
{
    if (!_sync_target_visible_index.has_value())
    {
        return;
    }

    const int viewport_line_count = std::max(1, _text_view_controller.viewport_line_count());
    const int visible_first = _text_view_controller.first_visible_line();
    const int visible_last = visible_first + viewport_line_count - 1;
    if (_sync_target_visible_index->value < visible_first || _sync_target_visible_index->value > visible_last)
    {
        _text_view_controller.center_on_line(_sync_target_visible_index->value);
    }
}

void LogController::stop_sync_selection()
{
    _sync_selection_active = false;
    _sync_selecting_destination = false;
    _sync_target_visible_index.reset();
    _sync_source_entry_index.reset();
    _sync_apply_handler = {};
}

void LogController::set_status_message(std::string message, bool is_error)
{
    _status_message = std::move(message);
    _status_is_error = is_error;
}

} // namespace slayerlog
