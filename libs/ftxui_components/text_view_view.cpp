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

    const int thumb_height = std::max(1, (viewport_lines * viewport_lines) / std::max(data.total_lines, viewport_lines));
    const int max_offset   = std::max(1, data.total_lines - viewport_lines);

    // DrawBlock gives 2 block-rows per terminal row, enabling sub-character thumb positioning.
    const int total_block_rows   = viewport_lines * 2;
    const int thumb_h_blocks     = thumb_height * 2;
    const int track_range_blocks = std::max(0, total_block_rows - thumb_h_blocks);
    const int thumb_top_block    = (data.first_visible_line * track_range_blocks) / max_offset;

    return ftxui::canvas(2, viewport_lines * 4,
                         [thumb_top_block, thumb_h_blocks, total_block_rows](ftxui::Canvas& canvas)
                         {
                             for (int blk = 0; blk < total_block_rows; ++blk)
                             {
                                 const bool        is_thumb = blk >= thumb_top_block && blk < (thumb_top_block + thumb_h_blocks);
                                 const ftxui::Color color   = is_thumb ? ftxui::Color::White : ftxui::Color::GrayDark;
                                 const int         y        = blk * 2;
                                 canvas.DrawBlock(0, y, true, color);
                                 canvas.DrawBlock(1, y, true, color);
                             }
                         });
}

ftxui::Element TextViewView::render_hscrollbar(const TextViewRenderData& data)
{
    const int viewport_cols = std::max(1, data.viewport_col_count);

    if (data.max_line_width <= viewport_cols)
    {
        return ftxui::text("") | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 0);
    }

    const int thumb_width  = std::max(1, (viewport_cols * viewport_cols) / std::max(data.max_line_width, viewport_cols));
    const int max_offset   = std::max(1, data.max_line_width - viewport_cols);

    // DrawBlock gives 2 block-columns per terminal column, enabling sub-character thumb positioning.
    const int total_block_cols   = viewport_cols * 2;
    const int thumb_w_blocks     = thumb_width * 2;
    const int track_range_blocks = std::max(0, total_block_cols - thumb_w_blocks);
    const int thumb_left_block   = (data.first_visible_col * track_range_blocks) / max_offset;

    // Use canvas height=2 (one block row) so the bar renders as upper half-blocks (▀),
    // matching the visual thickness of the 1-column-wide vertical scrollbar.
    return ftxui::canvas(viewport_cols * 2, 2,
                         [thumb_left_block, thumb_w_blocks, total_block_cols](ftxui::Canvas& canvas)
                         {
                             for (int blk = 0; blk < total_block_cols; ++blk)
                             {
                                 const bool         is_thumb = blk >= thumb_left_block && blk < (thumb_left_block + thumb_w_blocks);
                                 const ftxui::Color color    = is_thumb ? ftxui::Color::White : ftxui::Color::GrayDark;
                                 canvas.DrawBlock(blk, 0, true, color);
                             }
                         });
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
