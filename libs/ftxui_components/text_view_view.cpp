#include <ftxui_components/text_view_view.hpp>

#include <ftxui/dom/elements.hpp>

#include <algorithm>

namespace
{

int effective_viewport_line_count(const TextViewRenderData& data)
{
    return std::max(1, data.viewport_line_count);
}

void style_highlighted_columns(ftxui::Canvas& canvas, int row, const TextViewRenderData& data)
{
    const TextViewColumnHighlight& hl = data.col_highlight;

    if (!hl.active || hl.col_start >= hl.col_end)
    {
        return;
    }

    const int vp_start = std::max(0, hl.col_start - data.first_visible_col);
    const int vp_end   = std::min(std::max(1, data.viewport_col_count), std::max(0, hl.col_end - data.first_visible_col));

    if (vp_start >= vp_end)
    {
        return;
    }

    for (int col = vp_start; col < vp_end; ++col)
    {
        canvas.Style(col * 2, row * 4, [&](ftxui::Cell& cell) { cell.background_color = hl.color; });
    }
}

ftxui::Element render_content(const TextViewRenderData& data)
{
    const int viewport_lines = effective_viewport_line_count(data);
    const int viewport_cols  = std::max(1, data.viewport_col_count);

    return ftxui::canvas(viewport_cols * 2, viewport_lines * 4,
                         [data](ftxui::Canvas& canvas)
                         {
                             if (data.total_lines == 0)
                             {
                                 canvas.DrawText(0, 0, "<empty>", [](ftxui::Cell& cell) { cell.dim = true; });
                                 return;
                             }

                             for (int row = 0; row < static_cast<int>(data.visible_lines.size()); ++row)
                             {
                                 // Style the cell backgrounds first so the highlight also covers
                                 // blank space when the selected range extends past the line text.
                                 style_highlighted_columns(canvas, row, data);
                                 canvas.DrawText(0, row * 4, data.visible_lines[static_cast<std::size_t>(row)]);
                             }
                         });
}

} // namespace

TextViewView::TextViewView(TextViewController& controller) : _controller(controller)
{
}

ftxui::Element TextViewView::render_scrollbar(const TextViewRenderData& data)
{
    const int viewport_lines = effective_viewport_line_count(data);

    if (data.total_lines <= viewport_lines)
    {
        return ftxui::text("");
    }

    const int track_height = viewport_lines;
    const int thumb_height = std::max(1, (viewport_lines * viewport_lines) / std::max(data.total_lines, viewport_lines));
    const int max_offset   = std::max(1, data.total_lines - viewport_lines);
    const int track_range  = std::max(0, track_height - thumb_height);
    const int thumb_top    = (data.first_visible_line * track_range) / max_offset;

    ftxui::Elements track;
    track.reserve(static_cast<std::size_t>(track_height));
    for (int row = 0; row < track_height; ++row)
    {
        const bool is_thumb = row >= thumb_top && row < (thumb_top + thumb_height);
        track.push_back(ftxui::text(is_thumb ? "|" : ".") | (is_thumb ? ftxui::bold : ftxui::dim));
    }

    return ftxui::vbox(std::move(track));
}

ftxui::Element TextViewView::render_hscrollbar(const TextViewRenderData& data)
{
    const int viewport_cols = std::max(1, data.viewport_col_count);

    if (data.max_line_width <= viewport_cols)
    {
        return ftxui::text("") | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 0);
    }

    const int track_width = viewport_cols;
    const int thumb_width = std::max(1, (viewport_cols * viewport_cols) / std::max(data.max_line_width, viewport_cols));
    const int max_offset  = std::max(1, data.max_line_width - viewport_cols);
    const int track_range = std::max(0, track_width - thumb_width);
    const int thumb_left  = (data.first_visible_col * track_range) / max_offset;

    ftxui::Elements track;
    track.reserve(static_cast<std::size_t>(track_width));
    for (int col = 0; col < track_width; ++col)
    {
        const bool is_thumb = col >= thumb_left && col < (thumb_left + thumb_width);
        track.push_back(ftxui::text(is_thumb ? "-" : ".") | (is_thumb ? ftxui::bold : ftxui::dim));
    }

    return ftxui::hbox(std::move(track));
}

ftxui::Element TextViewView::component()
{
    const int box_height = std::max(1, (_content_box.y_max - _content_box.y_min) + 1);
    const int box_width  = std::max(1, (_content_box.x_max - _content_box.x_min) + 1);
    _controller.update_viewport_line_count(box_height);
    _controller.update_viewport_col_count(box_width);
    const TextViewRenderData data = _controller.render_data(box_height);
    return ftxui::vbox({
               ftxui::hbox({
                   render_content(data) | ftxui::flex | ftxui::reflect(_content_box),
                   render_scrollbar(data),
               }) | ftxui::flex,
               render_hscrollbar(data),
           }) |
           ftxui::reflect(_box);
}
