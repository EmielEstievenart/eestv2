#pragma once

#include <cstddef>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include "log_batch.hpp"
#include "log_types.hpp"

namespace slayerlog
{

class AllTrackedSources;

class ProcessedSources
{
public:
    void reset();

    void append_lines(const std::vector<ObservedLogLine>& lines);
    void append_batch(const LogBatch& batch);
    void replace_batch(const LogBatch& batch);

    void append_from_sources(const AllTrackedSources& tracked_sources, AllLineIndex first_new_entry_index);
    void rebuild_from_sources(const AllTrackedSources& tracked_sources);

    void toggle_pause();
    bool updates_paused() const;
    void set_show_source_labels(bool show_source_labels);
    void add_include_filter(std::string filter_text);
    void add_exclude_filter(std::string filter_text);
    void reset_filters();
    const std::vector<std::string>& include_filters() const;
    const std::vector<std::string>& exclude_filters() const;
    void hide_before_line_number(int line_number);
    std::optional<int> hidden_before_line_number() const;
    void hide_columns(int start_column, int end_column);
    void reset_hidden_columns();
    std::optional<HiddenColumnRange> hidden_columns() const;

    bool set_find_query(std::string query);
    void clear_find_query();
    bool find_active() const;
    const std::string& find_query() const;
    int total_find_match_count() const;
    int visible_find_match_count() const;
    std::optional<AllLineIndex> find_match_entry_index(FindResultIndex find_result_index) const;
    std::optional<FindResultIndex> find_match_position_for_entry_index(AllLineIndex entry_index) const;
    std::optional<VisibleLineIndex> visible_line_index_for_entry(AllLineIndex entry_index) const;
    std::optional<int> line_number_for_visible_line(VisibleLineIndex visible_line_index) const;
    std::optional<VisibleLineIndex> visible_line_index_for_line_number(int line_number) const;
    bool visible_line_matches_find(int visible_index) const;
    bool entry_index_is_visible(AllLineIndex entry_index) const;

    int line_count() const;
    int total_line_count() const;
    std::string rendered_line(int index) const;
    std::vector<std::string> rendered_lines(int first_index, int count) const;
    int max_rendered_line_width() const;

private:
    struct SearchPattern
    {
        std::string raw_text;
        std::string needle;
        std::optional<std::regex> regex;
    };

    std::string render_entry(AllLineIndex entry_index) const;
    void append_lines_immediately(const std::vector<ObservedLogLine>& lines);
    void flush_paused_updates();
    void rebuild_visible_entries();
    void expand_visible_entries(AllLineIndex first_new_entry_index);
    void rebuild_find_matches();
    void expand_find_matches(AllLineIndex first_new_entry_index);

    static SearchPattern compile_search_pattern(std::string_view text);
    bool entry_matches_find_query(const ObservedLogLine& entry) const;
    bool entry_matches_filters(const ObservedLogLine& entry) const;
    bool matches_pattern(std::string_view haystack, const SearchPattern& pattern) const;
    bool matches_any_pattern(std::string_view haystack, const std::vector<SearchPattern>& patterns) const;
    static std::string trim_filter_text(std::string_view text);
    std::string apply_hidden_columns(std::string text) const;

    IndexedVector<ObservedLogLine, AllLineIndex> _all_entries;
    IndexedVector<AllLineIndex, VisibleLineIndex> _visible_entry_indices;
    std::vector<ObservedLogLine> _paused_updates;

    std::vector<std::string> _include_filters;
    std::vector<std::string> _exclude_filters;
    std::vector<SearchPattern> _include_filter_patterns;
    std::vector<SearchPattern> _exclude_filter_patterns;

    std::string _find_query;
    std::optional<SearchPattern> _find_pattern;
    IndexedVector<AllLineIndex, FindResultIndex> _find_match_entry_indices;

    std::optional<int> _hidden_before_line_number;
    std::optional<HiddenColumnRange> _hidden_columns;

    bool _updates_paused     = false;
    bool _show_source_labels = false;
};

} // namespace slayerlog
