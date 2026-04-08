#include "text_view_view.hpp"
#include <ftxui/dom/elements.hpp>

#include <algorithm>

namespace
{

int effective_viewport_line_count(const TextViewRenderData& data)
{
    return std::max(1, data.viewport_line_count);
}

// Renders one visible line, applying a background-color highlight over the
// columns that fall within data.col_highlight (model-space).  The string
// `visible_text` already starts at `first_visible_col`, so the highlight range
// must be translated into that viewport-relative coordinate space before
// splitting the string into before / highlighted / after segments.
ftxui::Element render_highlighted_line(const std::string& visible_text, const TextViewRenderData& data)
{
    const TextViewColumnHighlight& hl = data.col_highlight;

    if (!hl.active || hl.col_start >= hl.col_end)
    {
        return ftxui::text(visible_text);
    }

    const int len      = static_cast<int>(visible_text.size());
    const int vp_start = std::max(0, hl.col_start - data.first_visible_col);
    const int vp_end   = std::min(len, std::max(0, hl.col_end - data.first_visible_col));

    if (vp_start >= vp_end)
    {
        return ftxui::text(visible_text);
    }

    ftxui::Elements parts;
    if (vp_start > 0)
    {
        parts.push_back(ftxui::text(visible_text.substr(0, static_cast<std::size_t>(vp_start))));
    }
    parts.push_back(ftxui::text(visible_text.substr(static_cast<std::size_t>(vp_start), static_cast<std::size_t>(vp_end - vp_start))) | ftxui::bgcolor(hl.color));
    if (vp_end < len)
    {
        parts.push_back(ftxui::text(visible_text.substr(static_cast<std::size_t>(vp_end))));
    }

    return ftxui::hbox(std::move(parts));
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
    ftxui::Elements rows;
    rows.reserve(static_cast<std::size_t>(box_height));
    if (data.total_lines == 0)
    {
        rows.push_back(ftxui::text("<empty>") | ftxui::dim);
    }
    else
    {
        for (const auto& line : data.visible_lines)
        {
            rows.push_back(render_highlighted_line(line, data));
        }
    }
    rows.push_back(ftxui::filler());
    return ftxui::vbox({
               ftxui::hbox({
                   ftxui::vbox(std::move(rows)) | ftxui::flex | ftxui::reflect(_content_box),
                   render_scrollbar(data),
               }) | ftxui::flex,
               render_hscrollbar(data),
           }) |
           ftxui::reflect(_box);
}
