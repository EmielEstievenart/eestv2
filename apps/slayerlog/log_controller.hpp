#pragma once

#include <optional>
#include <string>
#include <utility>

#include "log_view_model.hpp"

namespace slayerlog
{

class LogController
{
public:
    VisibleLineIndex first_visible_line_index(const LogViewModel& model, int viewport_line_count) const;

    void scroll_up(const LogViewModel& model, int viewport_line_count, int amount = 1);
    void scroll_down(const LogViewModel& model, int viewport_line_count, int amount = 1);
    void scroll_to_top(const LogViewModel& model, int viewport_line_count);
    void scroll_to_bottom();
    bool go_to_line(const LogViewModel& model, int line_number, int viewport_line_count);

    bool set_find_query(LogViewModel& model, std::string query, int viewport_line_count);
    void clear_find(LogViewModel& model);
    bool go_to_next_find_match(const LogViewModel& model, int viewport_line_count);
    bool go_to_previous_find_match(const LogViewModel& model, int viewport_line_count);
    std::optional<VisibleLineIndex> active_find_visible_index(const LogViewModel& model) const;

    void begin_selection(const LogViewModel& model, TextPosition position);
    void update_selection(const LogViewModel& model, TextPosition position);
    void end_selection(const LogViewModel& model, std::optional<TextPosition> position);
    void clear_selection();
    bool selection_in_progress() const;
    std::optional<std::pair<TextPosition, TextPosition>> selection_bounds(const LogViewModel& model) const;
    std::string selection_text(const LogViewModel& model) const;

private:
    int max_first_visible_line_index(const LogViewModel& model, int viewport_line_count) const;
    void center_on_visible_line(const LogViewModel& model, VisibleLineIndex target_visible_index, int viewport_line_count);
    TextPosition clamp_selection_position(const LogViewModel& model, TextPosition position) const;

    VisibleLineIndex _first_visible_line_index {0};
    bool _follow_bottom = true;

    std::optional<AllLineIndex> _active_find_entry_index;

    bool _selection_in_progress = false;
    std::optional<TextPosition> _selection_anchor;
    std::optional<TextPosition> _selection_focus;
};

} // namespace slayerlog
