#pragma once

#include <functional>
#include <ftxui/component/event.hpp>
#include <ftxui/screen/color.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <ftxui_components/text_view_model.hpp>

// Describes an optional background-color highlight over a contiguous column range.
// col_start is inclusive; col_end is exclusive (model-space column indices).
struct TextViewColumnHighlight
{
    int col_start      = 0;
    int col_end        = 0;
    ftxui::Color color = ftxui::Color::Red;
    bool active        = false;
};

struct TextViewStyle
{
    std::optional<ftxui::Color> foreground;
    std::optional<ftxui::Color> background;
    bool bold     = false;
    bool dim      = false;
    bool inverted = false;
};

struct TextViewLineDecoration
{
    int line_index = 0;
    TextViewStyle style;
};

struct TextViewRangeDecoration
{
    int line_index = 0;
    int col_start  = 0;
    int col_end    = 0;
    TextViewStyle style;
};

struct TextViewPosition
{
    int line_index = 0;
    int column     = 0;
};

struct TextViewRenderData
{
    int total_lines         = 0;
    int first_visible_line  = 0;
    int viewport_line_count = 1;
    int first_visible_col   = 0;
    int max_line_width      = 0;
    int viewport_col_count  = 1;
    TextViewColumnHighlight col_highlight;
    std::vector<std::string> visible_lines;
    std::vector<TextViewLineDecoration> line_decorations;
    std::vector<TextViewRangeDecoration> range_decorations;
};

// Result of handling an input event in the text view controller.
struct TextViewEventResult
{
    bool handled      = false;
    bool request_exit = false;
};

class TextViewController
{
public:
    explicit TextViewController(TextViewModel& model);

    // --- Content management ---

    // Switch the model to point at a different externally-owned vector.
    // Clamps scroll position, clears selection. Respects follow-bottom.
    void swap_lines(const std::vector<std::string>& new_lines);

    // Notify the controller that lines were appended to the current vector.
    // Handles follow-bottom auto-scroll.
    void notify_lines_appended();

    // --- Viewport ---

    void update_viewport_line_count(int viewport_line_count);
    void update_viewport_col_count(int viewport_col_count);

    // --- Scrolling ---

    void scroll_up(int amount);
    void scroll_down(int amount);
    void page_up();
    void page_down();
    void scroll_to_top();
    void scroll_to_bottom();
    void scroll_left(int amount);
    void scroll_right(int amount);

    // Scroll the viewport to center the given line index.
    void center_on_line(int line_index);

    // --- Column highlight ---

    void set_background_column_range(int col_start, int col_end, ftxui::Color color);
    void clear_background_column_range();

    // --- Text selection ---

    void begin_selection(TextViewPosition position);
    void update_selection(TextViewPosition position);
    void end_selection(std::optional<TextViewPosition> position);
    void clear_selection();

    [[nodiscard]] bool selection_in_progress() const;
    [[nodiscard]] std::optional<std::pair<TextViewPosition, TextViewPosition>> selection_bounds() const;
    [[nodiscard]] std::string selection_text() const;

    // --- Clipboard ---

    [[nodiscard]] bool copy_selection_to_clipboard() const;

    // --- Event handling ---

    // Dispatch a keyboard or mouse event.
    // mouse_to_text_position converts mouse screen coordinates to model-space text positions.
    TextViewEventResult parse_event(ftxui::Event event, const std::function<std::optional<TextViewPosition>(const ftxui::Mouse&)>& mouse_to_text_position = {});

    // --- Render ---

    // Build a complete render snapshot. Includes selection decorations.
    [[nodiscard]] TextViewRenderData render_data() const;

    // --- Accessors ---

    [[nodiscard]] int first_visible_line() const;
    [[nodiscard]] int first_visible_col() const;
    [[nodiscard]] bool follow_bottom() const;
    [[nodiscard]] int viewport_line_count() const;
    [[nodiscard]] int viewport_col_count() const;

private:
    [[nodiscard]] int normalize_viewport_line_count() const;
    [[nodiscard]] int max_first_visible_line() const;
    [[nodiscard]] int max_first_visible_col() const;
    void clamp_scroll_position();
    [[nodiscard]] TextViewPosition clamp_selection_position(TextViewPosition position) const;

    TextViewModel& _model;
    int _viewport_line_count = 1;
    int _first_visible_line  = 0;
    bool _follow_bottom      = true;
    int _first_visible_col   = 0;
    int _viewport_col_count  = 1;
    TextViewColumnHighlight _col_highlight;

    // Selection state
    bool _selection_in_progress = false;
    std::optional<TextViewPosition> _selection_anchor;
    std::optional<TextViewPosition> _selection_focus;
};
