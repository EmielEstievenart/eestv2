#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "file_watcher.hpp"

namespace slayerlog
{

struct TextPosition
{
    int line   = 0;
    int column = 0;
};

using LogTimePoint = std::chrono::system_clock::time_point;

struct ObservedLogUpdate
{
    std::string source_path;
    std::string source_label;
    FileWatcher::Update::Kind kind = FileWatcher::Update::Kind::Snapshot;
    std::vector<std::string> lines;
    LogTimePoint observed_at{};
    std::size_t source_index = 0;
    bool is_initial_load     = false;
    std::uint64_t poll_epoch = 0;
};

class LogViewModel
{
public:
    /** @brief Applies the startup snapshots as one batch so initial ordering can be resolved globally. */
    void apply_initial_updates(const std::vector<ObservedLogUpdate>& updates);
    /** @brief Applies a single live or replacement update while preserving the model ordering rules. */
    void apply_update(const ObservedLogUpdate& update);
    /** @brief Toggles update buffering so users can inspect the view without live movement. */
    void toggle_pause();
    /** @brief Returns whether incoming updates are currently buffered instead of rendered immediately. */
    bool updates_paused() const;
    /** @brief Enables source labels when multiple files are shown in the same view. */
    void set_show_source_labels(bool show_source_labels);

    /** @brief Sets the visible viewport height so scrolling and follow-bottom can be clamped correctly. */
    void set_visible_line_count(int count);
    /** @brief Returns the current viewport height in rendered lines. */
    int visible_line_count() const;
    /** @brief Returns the index of the first visible line. */
    int scroll_offset() const;
    /** @brief Returns the total number of rendered log lines in the model. */
    int line_count() const;

    /** @brief Scrolls upward and disables follow-bottom until the bottom is reached again. */
    void scroll_up(int amount = 1);
    /** @brief Scrolls downward and re-enables follow-bottom once the bottom is reached. */
    void scroll_down(int amount = 1);
    /** @brief Jumps to the first available line. */
    void scroll_to_top();
    /** @brief Jumps to the newest visible content. */
    void scroll_to_bottom();

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
    enum class SortPhase
    {
        InitialTimestamped,
        InitialUntimestamped,
        Live,
    };

    struct LogEntry
    {
        std::string source_path;
        std::string source_label;
        std::string text;
        SortPhase phase = SortPhase::InitialTimestamped;
        std::optional<LogTimePoint> anchor_timestamp;
        std::size_t source_index = 0;
        int source_group_order   = 0;
        int line_order           = 0;
        std::uint64_t poll_epoch = 0;
        std::uint64_t sequence = 0;
    };

    void append_entries_for_update(const ObservedLogUpdate& update);
    void apply_update_immediately(const ObservedLogUpdate& update);
    void flush_paused_updates();
    void sort_entries();
    void clamp_scroll_offset();
    int max_scroll_offset() const;
    void update_follow_bottom();
    void clamp_selection();

    std::vector<LogEntry> _entries;
    std::vector<ObservedLogUpdate> _paused_updates;
    std::uint64_t _next_sequence      = 0;
    int _scroll_offset            = 0;
    int _visible_line_count       = 1;
    bool _follow_bottom           = true;
    bool _updates_paused          = false;
    bool _show_source_labels      = false;
    bool _selection_in_progress   = false;
    std::optional<TextPosition> _selection_anchor;
    std::optional<TextPosition> _selection_focus;
};

} // namespace slayerlog
