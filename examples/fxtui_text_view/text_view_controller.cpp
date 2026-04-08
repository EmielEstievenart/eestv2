#include "text_view_controller.hpp"

#include <ftxui/component/event.hpp>
#include <ftxui/screen/color.hpp>

#include <algorithm>
#include <utility>

TextViewController::TextViewController(TextViewModel& model) : _model(model)
{
    update_generated_line_counter();
}

void TextViewController::update_viewport_line_count(int viewport_line_count)
{
    _viewport_line_count = normalize_viewport_line_count(viewport_line_count);
    if (_follow_bottom)
    {
        _first_visible_line = max_first_visible_line(_viewport_line_count);
        return;
    }

    clamp_scroll_position(_viewport_line_count);
}

void TextViewController::append_line(std::string line, int viewport_line_count)
{
    _model.append_line(std::move(line));
    update_generated_line_counter();
    if (_follow_bottom)
    {
        _first_visible_line = max_first_visible_line(viewport_line_count);
    }
    else
    {
        clamp_scroll_position(viewport_line_count);
    }
}

void TextViewController::append_lines(const std::vector<std::string>& lines, int viewport_line_count)
{
    _model.append_lines(lines);
    update_generated_line_counter();
    if (_follow_bottom)
    {
        _first_visible_line = max_first_visible_line(viewport_line_count);
    }
    else
    {
        clamp_scroll_position(viewport_line_count);
    }
}

void TextViewController::update_viewport_col_count(int viewport_col_count)
{
    _viewport_col_count = std::max(1, viewport_col_count);
    _first_visible_col  = std::min(_first_visible_col, max_first_visible_col());
}

void TextViewController::scroll_left(int amount)
{
    const int step     = std::max(1, amount);
    _first_visible_col = std::max(0, _first_visible_col - step);
}

void TextViewController::scroll_right(int amount)
{
    const int step     = std::max(1, amount);
    _first_visible_col = std::min(max_first_visible_col(), _first_visible_col + step);
}

void TextViewController::set_background_column_range(int col_start, int col_end, ftxui::Color color)
{
    _col_highlight.col_start = col_start;
    _col_highlight.col_end   = col_end;
    _col_highlight.color     = color;
    _col_highlight.active    = true;
}

void TextViewController::clear_background_column_range()
{
    _col_highlight.active = false;
}

void TextViewController::scroll_up(int amount, int viewport_line_count)
{
    const int step      = std::max(1, amount);
    _first_visible_line = std::max(0, _first_visible_line - step);
    _follow_bottom      = _first_visible_line >= max_first_visible_line(viewport_line_count);
}

void TextViewController::scroll_down(int amount, int viewport_line_count)
{
    const int step      = std::max(1, amount);
    _first_visible_line = std::min(max_first_visible_line(viewport_line_count), _first_visible_line + step);
    _follow_bottom      = _first_visible_line >= max_first_visible_line(viewport_line_count);
}

void TextViewController::page_up(int viewport_line_count)
{
    scroll_up(std::max(1, normalize_viewport_line_count(viewport_line_count) - 1), viewport_line_count);
}

void TextViewController::page_down(int viewport_line_count)
{
    scroll_down(std::max(1, normalize_viewport_line_count(viewport_line_count) - 1), viewport_line_count);
}

void TextViewController::scroll_to_top(int viewport_line_count)
{
    (void)viewport_line_count;
    _first_visible_line = 0;
    _follow_bottom      = false;
}

void TextViewController::scroll_to_bottom(int viewport_line_count)
{
    _first_visible_line = max_first_visible_line(viewport_line_count);
    _follow_bottom      = true;
}

bool TextViewController::parse_event(ftxui::Event event, const std::function<void()>& on_exit)
{
    if (event == ftxui::Event::Character('q') || event == ftxui::Event::Escape)
    {
        if (on_exit)
        {
            on_exit();
        }
        return true;
    }

    if (event == ftxui::Event::ArrowUp)
    {
        scroll_up(1, _viewport_line_count);
        return true;
    }

    if (event == ftxui::Event::ArrowDown)
    {
        scroll_down(1, _viewport_line_count);
        return true;
    }

    if (event == ftxui::Event::ArrowLeft)
    {
        scroll_left(1);
        return true;
    }

    if (event == ftxui::Event::ArrowRight)
    {
        scroll_right(1);
        return true;
    }

    if (event == ftxui::Event::PageUp)
    {
        page_up(_viewport_line_count);
        return true;
    }

    if (event == ftxui::Event::PageDown)
    {
        page_down(_viewport_line_count);
        return true;
    }

    if (event == ftxui::Event::Home)
    {
        scroll_to_top(_viewport_line_count);
        return true;
    }

    if (event == ftxui::Event::End)
    {
        scroll_to_bottom(_viewport_line_count);
        return true;
    }

    if (event == ftxui::Event::Character('a') || event == ftxui::Event::Character('A'))
    {
        append_line("Line " + std::to_string(_generated_line_counter) + " - appended from controller.", _viewport_line_count);
        return true;
    }

    if (event.is_mouse())
    {
        if (event.mouse().button == ftxui::Mouse::WheelUp)
        {
            scroll_up(1, _viewport_line_count);
            return true;
        }

        if (event.mouse().button == ftxui::Mouse::WheelDown)
        {
            scroll_down(1, _viewport_line_count);
            return true;
        }
    }

    return false;
}

TextViewRenderData TextViewController::render_data(int viewport_line_count) const
{
    const int safe_viewport = normalize_viewport_line_count(viewport_line_count);
    const int total         = _model.line_count();
    const int max_first     = std::max(0, total - safe_viewport);
    const int first         = std::clamp(_first_visible_line, 0, max_first);

    const int safe_col_viewport = std::max(1, _viewport_col_count);
    const int first_col         = std::max(0, _first_visible_col);

    TextViewRenderData data;
    data.total_lines         = total;
    data.first_visible_line  = first;
    data.viewport_line_count = safe_viewport;
    data.first_visible_col   = first_col;
    data.max_line_width      = _model.max_line_width();
    data.viewport_col_count  = safe_col_viewport;
    data.col_highlight       = _col_highlight;

    const int end = std::min(total, first + safe_viewport);
    data.visible_lines.reserve(static_cast<std::size_t>(std::max(0, end - first)));
    for (int index = first; index < end; ++index)
    {
        const std::string& line = _model.line_at(index);
        const int line_len      = static_cast<int>(line.size());
        if (first_col >= line_len)
        {
            data.visible_lines.emplace_back();
        }
        else
        {
            const int visible = std::min(line_len - first_col, safe_col_viewport);
            data.visible_lines.push_back(line.substr(static_cast<std::size_t>(first_col), static_cast<std::size_t>(visible)));
        }
    }

    return data;
}

int TextViewController::normalize_viewport_line_count(int viewport_line_count) const
{
    return std::max(1, viewport_line_count);
}

int TextViewController::max_first_visible_line(int viewport_line_count) const
{
    return std::max(0, _model.line_count() - normalize_viewport_line_count(viewport_line_count));
}

int TextViewController::max_first_visible_col() const
{
    return std::max(0, _model.max_line_width() - _viewport_col_count);
}

void TextViewController::update_generated_line_counter()
{
    _generated_line_counter = _model.line_count() + 1;
}

void TextViewController::clamp_scroll_position(int viewport_line_count)
{
    _first_visible_line = std::clamp(_first_visible_line, 0, max_first_visible_line(viewport_line_count));
}
