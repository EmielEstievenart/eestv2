#pragma once

#include <cstddef>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "log_batch.hpp"

namespace slayerlog
{

struct VisibleLineIndex
{
    int value = 0;
};

inline bool operator==(VisibleLineIndex lhs, VisibleLineIndex rhs)
{
    return lhs.value == rhs.value;
}

inline bool operator<(VisibleLineIndex lhs, VisibleLineIndex rhs)
{
    return lhs.value < rhs.value;
}

struct AllLineIndex
{
    int value = 0;
};

inline bool operator==(AllLineIndex lhs, AllLineIndex rhs)
{
    return lhs.value == rhs.value;
}

inline bool operator<(AllLineIndex lhs, AllLineIndex rhs)
{
    return lhs.value < rhs.value;
}

struct FindResultIndex
{
    int value = 0;
};

inline bool operator==(FindResultIndex lhs, FindResultIndex rhs)
{
    return lhs.value == rhs.value;
}

inline bool operator<(FindResultIndex lhs, FindResultIndex rhs)
{
    return lhs.value < rhs.value;
}

template <typename T, typename Index>
class IndexedVector
{
public:
    using iterator       = typename std::vector<T>::iterator;
    using const_iterator = typename std::vector<T>::const_iterator;

    T& operator[](Index index) { return _items[static_cast<std::size_t>(index.value)]; }

    const T& operator[](Index index) const { return _items[static_cast<std::size_t>(index.value)]; }

    void clear() { _items.clear(); }

    void reserve(std::size_t count) { _items.reserve(count); }

    void push_back(const T& value) { _items.push_back(value); }

    void push_back(T&& value) { _items.push_back(std::move(value)); }

    [[nodiscard]] std::size_t size() const { return _items.size(); }

    [[nodiscard]] bool empty() const { return _items.empty(); }

    iterator begin() { return _items.begin(); }

    iterator end() { return _items.end(); }

    const_iterator begin() const { return _items.begin(); }

    const_iterator end() const { return _items.end(); }

    const_iterator cbegin() const { return _items.cbegin(); }

    const_iterator cend() const { return _items.cend(); }

private:
    std::vector<T> _items;
};

struct TextPosition
{
    int line   = 0;
    int column = 0;
};

struct HiddenColumnRange
{
    int start = 0;
    int end   = 0;
};

inline bool operator==(const HiddenColumnRange& lhs, const HiddenColumnRange& rhs)
{
    return lhs.start == rhs.start && lhs.end == rhs.end;
}

std::optional<HiddenColumnRange> parse_hidden_column_range(std::string_view text);

class LogModel
{
public:
    /** @brief Resets the model to its initial empty state. */
    void reset();

    /** @brief Appends already ordered lines to the rendered log view. */
    void append_lines(const std::vector<ObservedLogLine>& lines);
    /** @brief Merges and appends a tracked-source batch to the rendered log view. */
    void append_batch(const LogBatch& batch);
    /** @brief Replaces loaded lines from a tracked-source batch while preserving session state. */
    void replace_batch(const LogBatch& batch);
    /** @brief Toggles update buffering so users can inspect the view without live movement. */
    void toggle_pause();
    /** @brief Returns whether incoming updates are currently buffered instead of rendered immediately. */
    bool updates_paused() const;
    /** @brief Enables source labels when multiple files are shown in the same view. */
    void set_show_source_labels(bool show_source_labels);
    /** @brief Adds an include filter and rebuilds the visible log. */
    void add_include_filter(std::string filter_text);
    /** @brief Adds an exclude filter and rebuilds the visible log. */
    void add_exclude_filter(std::string filter_text);
    /** @brief Removes every active filter and restores the full log view. */
    void reset_filters();
    /** @brief Returns active include filters in registration order. */
    const std::vector<std::string>& include_filters() const;
    /** @brief Returns active exclude filters in registration order. */
    const std::vector<std::string>& exclude_filters() const;
    /** @brief Hides all raw lines before the provided 1-based line number. */
    void hide_before_line_number(int line_number);
    /** @brief Returns the active raw-line cutoff, if any. */
    std::optional<int> hidden_before_line_number() const;
    /** @brief Hides displayed columns in the half-open range [start_column, end_column). */
    void hide_columns(int start_column, int end_column);
    /** @brief Clears the active displayed-column hide range. */
    void reset_hidden_columns();
    /** @brief Returns the active displayed-column hide range, if any. */
    std::optional<HiddenColumnRange> hidden_columns() const;

    /** @brief Sets the active find query and rebuilds find results. */
    bool set_find_query(std::string query);
    /** @brief Clears the active find query and all find results. */
    void clear_find_query();
    /** @brief Returns whether find mode is currently active. */
    bool find_active() const;
    /** @brief Returns the currently active find query text. */
    const std::string& find_query() const;
    /** @brief Returns total matches across all loaded lines. */
    int total_find_match_count() const;
    /** @brief Returns matches that are currently visible in the rendered model. */
    int visible_find_match_count() const;
    /** @brief Returns the entry index for a find result position. */
    std::optional<AllLineIndex> find_match_entry_index(FindResultIndex find_result_index) const;
    /** @brief Returns the find result position for an entry index. */
    std::optional<FindResultIndex> find_match_position_for_entry_index(AllLineIndex entry_index) const;
    /** @brief Returns the visible index for an entry index, if currently visible. */
    std::optional<VisibleLineIndex> visible_line_index_for_entry(AllLineIndex entry_index) const;
    /** @brief Returns the 1-based raw line number for a visible line index. */
    std::optional<int> line_number_for_visible_line(VisibleLineIndex visible_line_index) const;
    /** @brief Returns the visible index for a 1-based raw line number, if currently visible. */
    std::optional<VisibleLineIndex> visible_line_index_for_line_number(int line_number) const;
    /** @brief Returns whether a visible line index is a find match. */
    bool visible_line_matches_find(int visible_index) const;
    /** @brief Returns whether an entry index is currently visible. */
    bool entry_index_is_visible(AllLineIndex entry_index) const;

    /** @brief Returns the total number of rendered log lines in the model. */
    int line_count() const;
    /** @brief Returns the total number of observed log lines before filtering. */
    int total_line_count() const;

    /** @brief Returns a fully rendered line including line number and optional source label. */
    std::string rendered_line(int index) const;
    /** @brief Returns a contiguous slice of fully rendered visible lines. */
    std::vector<std::string> rendered_lines(int first_index, int count) const;
    /** @brief Returns the maximum width of the fully rendered visible lines. */
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
