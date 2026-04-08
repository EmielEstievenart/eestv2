#pragma once

#include <functional>
#include <ftxui/component/event.hpp>
#include <ftxui/screen/color.hpp>
#include <string>
#include <vector>

#include "text_view_model.hpp"

// Describes an optional background-color highlight over a contiguous column range.
// col_start is inclusive; col_end is exclusive (model-space column indices).
struct TextViewColumnHighlight
{
    int col_start      = 0;
    int col_end        = 0;
    ftxui::Color color = ftxui::Color::Red;
    bool active        = false;
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
};

class TextViewController
{
public:
    explicit TextViewController(TextViewModel& model);

    void update_viewport_line_count(int viewport_line_count);
    void update_viewport_col_count(int viewport_col_count);

    void append_line(std::string line, int viewport_line_count);
    void append_lines(const std::vector<std::string>& lines, int viewport_line_count);

    void scroll_up(int amount, int viewport_line_count);
    void scroll_down(int amount, int viewport_line_count);
    void page_up(int viewport_line_count);
    void page_down(int viewport_line_count);
    void scroll_to_top(int viewport_line_count);
    void scroll_to_bottom(int viewport_line_count);
    void scroll_left(int amount);
    void scroll_right(int amount);

    void set_background_column_range(int col_start, int col_end, ftxui::Color color);
    void clear_background_column_range();

    bool parse_event(ftxui::Event event, const std::function<void()>& on_exit);

    [[nodiscard]] TextViewRenderData render_data(int viewport_line_count) const;

private:
    [[nodiscard]] int normalize_viewport_line_count(int viewport_line_count) const;
    [[nodiscard]] int max_first_visible_line(int viewport_line_count) const;
    [[nodiscard]] int max_first_visible_col() const;
    void update_generated_line_counter();
    void clamp_scroll_position(int viewport_line_count);

    TextViewModel& _model;
    int _viewport_line_count    = 1;
    int _generated_line_counter = 1;
    int _first_visible_line     = 0;
    bool _follow_bottom         = true;
    int _first_visible_col      = 0;
    int _viewport_col_count     = 1;
    TextViewColumnHighlight _col_highlight;
};
