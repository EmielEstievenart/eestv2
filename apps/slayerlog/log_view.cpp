#include "log_view.hpp"
#include "view_theme.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace slayerlog
{

namespace
{

int max_line_width(const std::vector<std::string>& lines)
{
    int width = 0;
    for (const auto& line : lines)
    {
        width = std::max(width, static_cast<int>(line.size()));
    }

    return width;
}

std::vector<std::string> clip_lines_horizontally(std::vector<std::string> lines, int first_visible_col, int viewport_col_count)
{
    for (auto& line : lines)
    {
        if (first_visible_col >= static_cast<int>(line.size()))
        {
            line.clear();
            continue;
        }

        line = line.substr(static_cast<std::size_t>(first_visible_col), static_cast<std::size_t>(viewport_col_count));
    }

    return lines;
}

// Approximate viewport size for the first render before FTXUI's reflect() has measured
// the actual box. Breakdown: window border (2) + header (1) + separators (2) + status lines (3).
// The value 7 slightly underestimates, which is acceptable as a fallback for a single frame.
constexpr int window_chrome_height = 7;

int estimate_visible_line_count(int viewport_line_count, int screen_height)
{
    if (viewport_line_count > 0)
    {
        return viewport_line_count;
    }

    return std::max(1, screen_height - window_chrome_height);
}

std::string join(const std::vector<std::string>& items, const std::string& separator = ", ")
{
    std::string result;
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        if (i > 0)
        {
            result += separator;
        }
        result += items[i];
    }
    return result;
}

ftxui::Element build_filter_status(const LogModel& model)
{
    ftxui::Elements parts;
    parts.push_back(theme::badge("FILTER", theme::label_filter_fg));

    const auto hidden_before  = model.hidden_before_line_number();
    const auto hidden_columns = model.hidden_columns();

    if (model.include_filters().empty() && model.exclude_filters().empty() && !hidden_before.has_value() && !hidden_columns.has_value())
    {
        parts.push_back(ftxui::text(" none") | ftxui::color(theme::muted));
        return ftxui::hbox(std::move(parts));
    }

    if (!model.include_filters().empty())
    {
        parts.push_back(ftxui::text(" in(" + join(model.include_filters()) + ")"));
    }

    if (!model.exclude_filters().empty())
    {
        parts.push_back(ftxui::text(" out(" + join(model.exclude_filters()) + ")"));
    }

    if (hidden_before.has_value())
    {
        parts.push_back(ftxui::text(" | before line " + std::to_string(*hidden_before)) | ftxui::color(theme::muted));
    }

    if (hidden_columns.has_value())
    {
        parts.push_back(ftxui::text(" | columns " + std::to_string(hidden_columns->start) + "-" + std::to_string(hidden_columns->end)) | ftxui::color(theme::muted));
    }

    return ftxui::hbox(std::move(parts));
}

ftxui::Element build_find_status(const LogModel& model, const LogController& controller)
{
    ftxui::Elements parts;
    parts.push_back(theme::badge("FIND", theme::label_find_fg));

    if (!model.find_active())
    {
        parts.push_back(ftxui::text(" off") | ftxui::color(theme::muted));
        return ftxui::hbox(std::move(parts));
    }

    parts.push_back(ftxui::text(" \"" + model.find_query() + "\""));
    parts.push_back(ftxui::text(" " + std::to_string(model.visible_find_match_count()) + "/" + std::to_string(model.total_find_match_count()) + " matches") | ftxui::color(theme::muted));

    const auto active_visible_index = controller.active_find_visible_index(model);
    if (active_visible_index.has_value())
    {
        parts.push_back(ftxui::text(" | line " + std::to_string(active_visible_index->value + 1)) | ftxui::color(theme::muted));
    }

    return ftxui::hbox(std::move(parts));
}

ftxui::Element build_key_hints()
{
    auto sep = []() { return ftxui::text("  "); };
    return ftxui::hbox({
        theme::key_hint("Ctrl+P", "commands"),
        sep(),
        theme::key_hint("Ctrl+R", "history"),
        sep(),
        theme::key_hint("\xe2\x86\x92", "next"),
        sep(),
        theme::key_hint("\xe2\x86\x90", "prev"),
        sep(),
        theme::key_hint("Esc", "close find"),
        sep(),
        theme::key_hint("q", "quit"),
    });
}

TextViewStyle make_find_style(bool is_active_match)
{
    TextViewStyle style;
    style.background = is_active_match ? theme::find_active_bg : theme::find_match_bg;
    style.foreground = is_active_match ? theme::find_active_fg : theme::find_match_fg;
    return style;
}

TextViewRenderData build_text_view_data(const LogModel& model, const LogController& controller, int viewport_line_count, int viewport_col_count, std::optional<HiddenColumnRange> hidden_column_preview)
{
    TextViewRenderData data;
    data.first_visible_line  = controller.first_visible_line_index(model, viewport_line_count).value;
    data.viewport_line_count = viewport_line_count;
    data.first_visible_col   = controller.first_visible_col(model, viewport_col_count);
    data.viewport_col_count  = viewport_col_count;

    if (model.line_count() == 0)
    {
        data.total_lines = 1;
        data.visible_lines.push_back("1 " + std::string(model.total_line_count() == 0 ? "<empty file>" : "<no matching lines>"));
        data.max_line_width = max_line_width(data.visible_lines);
        TextViewLineDecoration decoration;
        decoration.line_index = 0;
        decoration.style.dim  = true;
        data.line_decorations.push_back(decoration);
        return data;
    }

    data.total_lines    = model.line_count();
    data.visible_lines  = clip_lines_horizontally(model.rendered_lines(data.first_visible_line, viewport_line_count), data.first_visible_col, data.viewport_col_count);
    data.max_line_width = model.max_rendered_line_width();

    if (hidden_column_preview.has_value())
    {
        data.col_highlight.col_start = hidden_column_preview->start;
        data.col_highlight.col_end   = hidden_column_preview->end;
        data.col_highlight.color     = theme::hidden_columns_preview_bg;
        data.col_highlight.active    = true;
    }

    const auto active_find_index = controller.active_find_visible_index(model);
    for (std::size_t offset = 0; offset < data.visible_lines.size(); ++offset)
    {
        const int line_index      = data.first_visible_line + static_cast<int>(offset);
        const bool is_find_match  = model.find_active() && model.visible_line_matches_find(line_index);
        const bool is_active_find = active_find_index.has_value() && active_find_index->value == line_index;

        if (is_find_match)
        {
            TextViewLineDecoration decoration;
            decoration.line_index = line_index;
            decoration.style      = make_find_style(is_active_find);
            data.line_decorations.push_back(decoration);
        }
    }

    const auto selected_range = controller.selection_bounds(model);
    if (!selected_range.has_value())
    {
        return data;
    }

    for (int line_index = selected_range->first.line; line_index <= selected_range->second.line; ++line_index)
    {
        if (line_index < data.first_visible_line || line_index >= (data.first_visible_line + static_cast<int>(data.visible_lines.size())))
        {
            continue;
        }

        const auto rendered_line  = model.rendered_line(line_index);
        const int selection_start = (line_index == selected_range->first.line) ? selected_range->first.column : 0;
        const int selection_end   = (line_index == selected_range->second.line) ? selected_range->second.column : static_cast<int>(rendered_line.size());
        const int clamped_start   = std::clamp(selection_start, 0, static_cast<int>(rendered_line.size()));
        const int clamped_end     = std::clamp(selection_end, clamped_start, static_cast<int>(rendered_line.size()));
        if (clamped_start == clamped_end)
        {
            continue;
        }

        TextViewStyle style;
        style.inverted = true;
        TextViewRangeDecoration decoration;
        decoration.line_index = line_index;
        decoration.col_start  = clamped_start;
        decoration.col_end    = clamped_end;
        decoration.style      = style;
        data.range_decorations.push_back(decoration);
    }

    return data;
}

} // namespace

ftxui::Element LogView::render(const LogModel& model, const LogController& controller, const std::string& header_text, int screen_height, std::optional<HiddenColumnRange> hidden_column_preview)
{
    const int visible_line_count = estimate_visible_line_count(_text_view.viewport_line_count(), screen_height);
    const int visible_col_count  = std::max(1, _text_view.viewport_col_count());
    auto log_view                = _text_view.render(build_text_view_data(model, controller, visible_line_count, visible_col_count, hidden_column_preview)) | ftxui::flex;

    // Header with optional paused indicator
    ftxui::Element header;
    if (model.updates_paused())
    {
        header = ftxui::hbox({
            ftxui::text(header_text) | ftxui::bold,
            ftxui::text(" "),
            theme::badge("PAUSED", theme::paused_fg),
        });
    }
    else
    {
        header = ftxui::text(header_text) | ftxui::bold;
    }

    return ftxui::window(ftxui::text("Slayerlog"), ftxui::vbox({
                                                       header,
                                                       ftxui::separator(),
                                                       log_view,
                                                       ftxui::separator(),
                                                       build_filter_status(model),
                                                       build_find_status(model, controller),
                                                       build_key_hints(),
                                                   }));
}

int LogView::visible_line_count(int screen_height) const
{
    return estimate_visible_line_count(_text_view.viewport_line_count(), screen_height);
}

int LogView::visible_col_count() const
{
    return std::max(1, _text_view.viewport_col_count());
}

std::optional<TextPosition> LogView::mouse_to_text_position(const LogModel& model, const LogController& controller, const ftxui::Mouse& mouse) const
{
    if (model.line_count() == 0)
    {
        return std::nullopt;
    }

    const int viewport_line_count = visible_line_count(1);
    const int viewport_col_count  = std::max(1, _text_view.viewport_col_count());
    const auto position           = _text_view.mouse_to_text_position(build_text_view_data(model, controller, viewport_line_count, viewport_col_count, std::nullopt), mouse);
    if (!position.has_value() || position->line_index < 0 || position->line_index >= model.line_count())
    {
        return std::nullopt;
    }

    return TextPosition {
        position->line_index,
        position->column,
    };
}

} // namespace slayerlog
