#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>

#include <ftxui_components/text_view_controller.hpp>
#include <ftxui_components/text_view_model.hpp>

#include "processed_sources.hpp"

namespace slayerlog
{

struct LogEventResult
{
    bool handled      = false;
    bool request_exit = false;
};

class LogController
{
public:
    LogController();

    void reset();

    // --- Content management ---

    // Full rebuild: renders all visible lines into inactive buffer, swaps to active.
    // Call after filter changes, model resets, replace_batch, hide_columns, etc.
    void rebuild_view(const ProcessedSources& processed_sources);

    // Incremental update: appends new rendered lines to active buffer.
    // Call after append_batch / append_lines for streaming.
    void sync_view(const ProcessedSources& processed_sources);

    // --- Domain-specific navigation ---

    bool go_to_line(const ProcessedSources& processed_sources, int line_number);

    // --- Find ---

    bool set_find_query(ProcessedSources& processed_sources, std::string query);
    void clear_find(ProcessedSources& processed_sources);
    bool go_to_next_find_match(const ProcessedSources& processed_sources);
    bool go_to_previous_find_match(const ProcessedSources& processed_sources);
    std::optional<VisibleLineIndex> active_find_visible_index(const ProcessedSources& processed_sources) const;

    // --- Event handling ---

    LogEventResult handle_event(ProcessedSources& processed_sources, ftxui::Event event, const std::function<std::optional<TextViewPosition>(const ftxui::Mouse&)>& mouse_to_text_position);

    // --- Access to underlying text view ---

    TextViewController& text_view_controller();
    const TextViewController& text_view_controller() const;
    const TextViewModel& text_view_model() const;

private:
    std::vector<std::string>& active_buffer();
    std::vector<std::string>& inactive_buffer();

    TextViewModel _text_view_model;
    TextViewController _text_view_controller;

    std::vector<std::string> _buffer_a;
    std::vector<std::string> _buffer_b;
    bool _active_buffer_is_a = true;
    int _synced_line_count   = 0;

    std::optional<AllLineIndex> _active_find_entry_index;
};

} // namespace slayerlog
