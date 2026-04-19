#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "log_batch.hpp"
#include "log_types.hpp"
#include "search_pattern.hpp"

namespace slayerlog
{

class AllTrackedSources;

class AllProcessedSources
{
public:
    struct FilterSelection
    {
        bool include      = true;
        std::size_t index = 0;
        std::string text;
    };

    void reset();

    void append_lines(const std::vector<LogEntry>& lines);
    void append_batch(const std::vector<LogEntry>& batch);
    void replace_batch(const std::vector<LogEntry>& batch);

    void append_from_sources(const AllTrackedSources& tracked_sources, AllLineIndex first_new_entry_index);
    void rebuild_from_sources(const AllTrackedSources& tracked_sources);

    void toggle_pause();
    bool updates_paused() const;
    void set_show_source_labels(bool show_source_labels);
    void add_include_filter(std::string filter_text);
    void add_exclude_filter(std::string filter_text);
    void reset_filters();
    bool remove_filters(const std::vector<FilterSelection>& filters);
    const std::vector<std::string>& include_filters() const;
    const std::vector<std::string>& exclude_filters() const;
    std::vector<FilterSelection> all_filters() const;
    void hide_before_line_number(int line_number);
    std::optional<int> hidden_before_line_number() const;
    void hide_columns(int start_column, int end_column);
    void reset_hidden_columns();
    std::optional<HiddenColumnRange> hidden_columns() const;

    const LogEntry& entry_at(AllLineIndex entry_index) const;
    std::optional<AllLineIndex> entry_index_for_visible_line(VisibleLineIndex visible_line_index) const;
    std::optional<VisibleLineIndex> visible_line_index_for_entry(AllLineIndex entry_index) const;
    std::optional<int> line_number_for_visible_line(VisibleLineIndex visible_line_index) const;
    std::optional<VisibleLineIndex> visible_line_index_for_line_number(int line_number) const;
    bool entry_index_is_visible(AllLineIndex entry_index) const;

    int line_count() const;
    int total_line_count() const;
    std::string rendered_line(int index) const;
    std::vector<std::string> rendered_lines(int first_index, int count) const;
    int max_rendered_line_width() const;

private:
    std::string render_entry(AllLineIndex entry_index) const;
    void append_lines_immediately(const std::vector<LogEntry>& lines);
    void flush_paused_updates();
    void rebuild_visible_entries();
    void expand_visible_entries(AllLineIndex first_new_entry_index);
    bool entry_matches_filters(const LogEntry& entry) const;
    std::string apply_hidden_columns(std::string text) const;

    IndexedVector<LogEntry, AllLineIndex> _all_entries;
    IndexedVector<AllLineIndex, VisibleLineIndex> _visible_entry_indices;
    std::vector<LogEntry> _paused_updates;

    std::vector<std::string> _include_filters;
    std::vector<std::string> _exclude_filters;
    std::vector<SearchPattern> _include_filter_patterns;
    std::vector<SearchPattern> _exclude_filter_patterns;

    std::optional<int> _hidden_before_line_number;
    std::optional<HiddenColumnRange> _hidden_columns;

    bool _updates_paused     = false;
    bool _show_source_labels = false;
};

} // namespace slayerlog
