#pragma once

#include <functional>
#include <optional>
#include <string>
#include <utility>

#ifdef _WIN32
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#endif

#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>

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
    void reset();

    VisibleLineIndex first_visible_line_index(const LogModel& model, int viewport_line_count) const;

    void scroll_up(const LogModel& model, int viewport_line_count, int amount = 1);
    void scroll_down(const LogModel& model, int viewport_line_count, int amount = 1);
    void scroll_to_top(const LogModel& model, int viewport_line_count);
    void scroll_to_bottom();
    bool go_to_line(const LogModel& model, int line_number, int viewport_line_count);

    bool set_find_query(LogModel& model, std::string query, int viewport_line_count);
    void clear_find(LogModel& model);
    bool go_to_next_find_match(const LogModel& model, int viewport_line_count);
    bool go_to_previous_find_match(const LogModel& model, int viewport_line_count);
    std::optional<VisibleLineIndex> active_find_visible_index(const LogModel& model) const;

    void begin_selection(const LogModel& model, TextPosition position);
    void update_selection(const LogModel& model, TextPosition position);
    void end_selection(const LogModel& model, std::optional<TextPosition> position);
    void clear_selection();
    bool selection_in_progress() const;
    std::optional<std::pair<TextPosition, TextPosition>> selection_bounds(const LogModel& model) const;
    std::string selection_text(const LogModel& model) const;

    LogEventResult handle_event(LogModel& model, ftxui::Event event, int viewport_line_count,
                                const std::function<std::optional<TextPosition>(const ftxui::Mouse& mouse)>& mouse_to_text_position);

private:
    bool copy_selection_to_clipboard(const LogModel& model) const;

    int max_first_visible_line_index(const LogModel& model, int viewport_line_count) const;
    void center_on_visible_line(const LogModel& model, VisibleLineIndex target_visible_index, int viewport_line_count);
    TextPosition clamp_selection_position(const LogModel& model, TextPosition position) const;

    VisibleLineIndex _first_visible_line_index {0};
    bool _follow_bottom = true;

    std::optional<AllLineIndex> _active_find_entry_index;

    bool _selection_in_progress = false;
    std::optional<TextPosition> _selection_anchor;
    std::optional<TextPosition> _selection_focus;
};

} // namespace slayerlog
