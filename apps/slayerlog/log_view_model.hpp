#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "log_batch.hpp"

namespace slayerlog
{

struct TextPosition
{
    int line   = 0;
    int column = 0;
};

class LogViewModel
{
public:
    /** @brief Appends already ordered lines to the rendered log view. */
    void append_lines(const std::vector<ObservedLogLine>& lines);
    /** @brief Toggles update buffering so users can inspect the view without live movement. */
    void toggle_pause();
    /** @brief Returns whether incoming updates are currently buffered instead of rendered immediately. */
    bool updates_paused() const;
    /** @brief Enables source labels when multiple files are shown in the same view. */
    void set_show_source_labels(bool show_source_labels);
    /** @brief Adds an include filter and rebuilds the visible log. */
    void add_include_filter(std::string filter_text);
    /** @brief Adds an exclude filter and rebuilds the visible log. */
    void add_exclude_filter(std::string filter_text);
    /** @brief Removes every active filter and restores the full log view. */
    void reset_filters();
    /** @brief Returns active include filters in registration order. */
    const std::vector<std::string>& include_filters() const;
    /** @brief Returns active exclude filters in registration order. */
    const std::vector<std::string>& exclude_filters() const;
    /** @brief Hides all raw lines before the provided 1-based line number. */
    void hide_before_line_number(int line_number);
    /** @brief Returns the active raw-line cutoff, if any. */
    std::optional<int> hidden_before_line_number() const;

    /** @brief Sets the visible viewport height so scrolling and follow-bottom can be clamped correctly. */
    void set_visible_line_count(int count);
    /** @brief Returns the current viewport height in rendered lines. */
    int visible_line_count() const;
    /** @brief Returns the index of the first visible line. */
    int scroll_offset() const;
    /** @brief Returns the total number of rendered log lines in the model. */
    int line_count() const;
    /** @brief Returns the total number of observed log lines before filtering. */
    int total_line_count() const;

    /** @brief Scrolls upward and disables follow-bottom until the bottom is reached again. */
    void scroll_up(int amount = 1);
    /** @brief Scrolls downward and re-enables follow-bottom once the bottom is reached. */
    void scroll_down(int amount = 1);
    /** @brief Jumps to the first available line. */
    void scroll_to_top();
    /** @brief Jumps to the newest visible content. */
    void scroll_to_bottom();
    /** @brief Centers the viewport on the requested 1-based line number when it is visible. */
    bool center_on_line_number(int line_number);

    /** @brief Starts a text selection at the given rendered position. */
    void begin_selection(TextPosition position);
    /** @brief Extends the active selection while dragging. */
    void update_selection(TextPosition position);
    /** @brief Finishes the selection, optionally updating the final focus position. */
    void end_selection(std::optional<TextPosition> position);
    /** @brief Clears the current selection state. */
    void clear_selection();
    /** @brief Returns whether a mouse-driven selection gesture is currently active. */
    bool selection_in_progress() const;
    /** @brief Returns normalized selection bounds for rendering and copying. */
    std::optional<std::pair<TextPosition, TextPosition>> selection_bounds() const;
    /** @brief Extracts the selected text from the rendered view. */
    std::string selection_text() const;

    /** @brief Returns a fully rendered line including line number and optional source label. */
    std::string rendered_line(int index) const;

private:
    void append_lines_immediately(const std::vector<ObservedLogLine>& lines);
    void flush_paused_updates();
    void rebuild_visible_entries();
    void clamp_scroll_offset();
    int max_scroll_offset() const;
    void update_follow_bottom();
    void clamp_selection();
    bool entry_matches_filters(const ObservedLogLine& entry) const;
    bool line_number_is_visible(int line_number) const;
    bool matches_any_filter(std::string_view haystack, const std::vector<std::string>& filters) const;
    static std::string trim_filter_text(std::string_view text);

    std::vector<ObservedLogLine> _all_entries;
    std::vector<int> _visible_entry_indices;
    std::vector<ObservedLogLine> _paused_updates;
    std::vector<std::string> _include_filters;
    std::vector<std::string> _exclude_filters;
    std::optional<int> _hidden_before_line_number;
    int _scroll_offset          = 0;
    int _visible_line_count     = 1;
    bool _follow_bottom         = true;
    bool _updates_paused        = false;
    bool _show_source_labels    = false;
    bool _selection_in_progress = false;
    std::optional<TextPosition> _selection_anchor;
    std::optional<TextPosition> _selection_focus;
};

} // namespace slayerlog
