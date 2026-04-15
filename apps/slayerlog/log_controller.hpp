#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>

#include <ftxui_components/text_view_controller.hpp>
#include <ftxui_components/text_view_model.hpp>

#include "log_model.hpp"

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
    void rebuild_view(const LogModel& model);

    // Incremental update: appends new rendered lines to active buffer.
    // Call after append_batch / append_lines for streaming.
    void sync_view(const LogModel& model);

    // --- Domain-specific navigation ---

    bool go_to_line(const LogModel& model, int line_number);

    // --- Find ---

    bool set_find_query(LogModel& model, std::string query);
    void clear_find(LogModel& model);
    bool go_to_next_find_match(const LogModel& model);
    bool go_to_previous_find_match(const LogModel& model);
    std::optional<VisibleLineIndex> active_find_visible_index(const LogModel& model) const;

    // --- Event handling ---

    LogEventResult handle_event(LogModel& model, ftxui::Event event, const std::function<std::optional<TextViewPosition>(const ftxui::Mouse&)>& mouse_to_text_position);

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
