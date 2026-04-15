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

// Approximate viewport size for the first render before FTXUI's reflect() has measured
// the actual box. Breakdown: window border (2) + header (1) + separators (2) + status lines (3).
// The value 7 slightly underestimates, which is acceptable as a fallback for a single frame.
constexpr int window_chrome_height = 7;

int estimate_viewport_line_count(int measured, int screen_height)
{
    if (measured > 0)
    {
        return measured;
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

} // namespace

ftxui::Element LogView::render(const LogModel& model, LogController& controller, const std::string& header_text, int screen_height, std::optional<HiddenColumnRange> hidden_column_preview)
{
    // Update viewport dimensions on the text view controller
    const int effective_height = estimate_viewport_line_count(_text_view.viewport_line_count(), screen_height);
    const int effective_width  = std::max(1, _text_view.viewport_col_count());
    controller.text_view_controller().update_viewport_line_count(effective_height);
    controller.text_view_controller().update_viewport_col_count(effective_width);

    // Get render data from the text view controller
    auto data = controller.text_view_controller().render_data();

    // Handle empty state
    if (model.line_count() == 0)
    {
        data.total_lines = 1;
        data.visible_lines.clear();
        data.visible_lines.push_back("1 " + std::string(model.total_line_count() == 0 ? "<empty file>" : "<no matching lines>"));
        data.max_line_width = static_cast<int>(data.visible_lines[0].size());
        data.line_decorations.clear();
        data.range_decorations.clear();
        TextViewLineDecoration decoration;
        decoration.line_index = 0;
        decoration.style.dim  = true;
        data.line_decorations.push_back(decoration);
    }
    else
    {
        // Add hidden column preview highlight
        if (hidden_column_preview.has_value())
        {
            data.col_highlight.col_start = hidden_column_preview->start;
            data.col_highlight.col_end   = hidden_column_preview->end;
            data.col_highlight.color     = theme::hidden_columns_preview_bg;
            data.col_highlight.active    = true;
        }

        // Add find decorations
        const auto active_find_index = controller.active_find_visible_index(model);
        for (std::size_t offset = 0; offset < data.visible_lines.size(); ++offset)
        {
            const int line_index      = data.first_visible_line + static_cast<int>(offset);
            const bool is_find_match  = model.find_active() && model.visible_line_matches_find(line_index);
            const bool is_active_find = active_find_index.has_value() && active_find_index->value == line_index;

            if (is_find_match)
            {
                TextViewLineDecoration decoration;
                decoration.line_index       = line_index;
                decoration.style.background = is_active_find ? theme::find_active_bg : theme::find_match_bg;
                decoration.style.foreground = is_active_find ? theme::find_active_fg : theme::find_match_fg;
                data.line_decorations.push_back(decoration);
            }
        }
    }

    auto log_view = _text_view.render(data) | ftxui::flex;

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

std::optional<TextViewPosition> LogView::mouse_to_text_position(const LogController& controller, const ftxui::Mouse& mouse) const
{
    if (controller.text_view_model().line_count() == 0)
    {
        return std::nullopt;
    }

    const auto data = controller.text_view_controller().render_data();
    return _text_view.mouse_to_text_position(data, mouse);
}

} // namespace slayerlog
