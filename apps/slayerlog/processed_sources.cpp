#include "processed_sources.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstddef>
#include <sstream>
#include <system_error>

#include "all_tracked_sources.hpp"
#include "search_pattern.hpp"

namespace slayerlog
{

namespace
{

std::string trim_text(std::string_view text)
{
    std::size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0)
    {
        ++start;
    }

    std::size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
    {
        --end;
    }

    return std::string(text.substr(start, end - start));
}

std::optional<int> parse_non_negative_integer(std::string_view text)
{
    const std::string trimmed = trim_text(text);
    if (trimmed.empty())
    {
        return std::nullopt;
    }

    int value               = 0;
    const auto [end, error] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), value);
    if (error != std::errc() || end != trimmed.data() + trimmed.size() || value < 0)
    {
        return std::nullopt;
    }

    return value;
}

} // namespace

std::optional<HiddenColumnRange> parse_hidden_column_range(std::string_view text)
{
    const std::string trimmed         = trim_text(text);
    const std::size_t separator_index = trimmed.find('-');
    if (separator_index == std::string::npos || trimmed.find('-', separator_index + 1) != std::string::npos)
    {
        return std::nullopt;
    }

    const auto start = parse_non_negative_integer(std::string_view(trimmed).substr(0, separator_index));
    const auto end   = parse_non_negative_integer(std::string_view(trimmed).substr(separator_index + 1));
    if (!start.has_value() || !end.has_value() || *end <= *start)
    {
        return std::nullopt;
    }

    return HiddenColumnRange {*start, *end};
}

void ProcessedSources::reset()
{
    _all_entries.clear();
    _visible_entry_indices.clear();
    _paused_updates.clear();

    _include_filters.clear();
    _exclude_filters.clear();
    _include_filter_patterns.clear();
    _exclude_filter_patterns.clear();

    _hidden_before_line_number.reset();
    _hidden_columns.reset();

    _updates_paused     = false;
    _show_source_labels = false;
}

void ProcessedSources::append_lines(const std::vector<ObservedLogLine>& lines)
{
    if (_updates_paused)
    {
        _paused_updates.insert(_paused_updates.end(), lines.begin(), lines.end());
    }
    else
    {
        append_lines_immediately(lines);
    }
}

void ProcessedSources::append_batch(const LogBatch& batch)
{
    append_lines(merge_log_batch(batch));
}

void ProcessedSources::replace_batch(const LogBatch& batch)
{
    const auto merged_lines = merge_log_batch(batch);

    _all_entries.clear();
    _visible_entry_indices.clear();
    _paused_updates.clear();

    _all_entries.reserve(merged_lines.size());
    for (const auto& line : merged_lines)
    {
        _all_entries.push_back(line);
    }

    rebuild_visible_entries();
}

void ProcessedSources::append_from_sources(const AllTrackedSources& tracked_sources, AllLineIndex first_new_entry_index)
{
    const auto& lines = tracked_sources.all_lines();
    if (first_new_entry_index.value < 0 || first_new_entry_index.value >= static_cast<int>(lines.size()))
    {
        return;
    }

    std::vector<ObservedLogLine> appended_lines;
    appended_lines.reserve(lines.size() - static_cast<std::size_t>(first_new_entry_index.value));
    for (int index = first_new_entry_index.value; index < static_cast<int>(lines.size()); ++index)
    {
        appended_lines.push_back(lines[AllLineIndex {index}]);
    }

    append_lines(appended_lines);
}

void ProcessedSources::rebuild_from_sources(const AllTrackedSources& tracked_sources)
{
    _all_entries.clear();
    _visible_entry_indices.clear();
    _paused_updates.clear();

    const auto& lines = tracked_sources.all_lines();
    _all_entries.reserve(lines.size());
    for (const auto& line : lines)
    {
        _all_entries.push_back(line);
    }

    rebuild_visible_entries();
}

void ProcessedSources::toggle_pause()
{
    _updates_paused = !_updates_paused;
    if (!_updates_paused)
    {
        flush_paused_updates();
    }
}

bool ProcessedSources::updates_paused() const
{
    return _updates_paused;
}

void ProcessedSources::set_show_source_labels(bool show_source_labels)
{
    _show_source_labels = show_source_labels;
}

void ProcessedSources::add_include_filter(std::string filter_text)
{
    filter_text = trim_search_text(filter_text);
    if (filter_text.empty())
    {
        return;
    }

    const SearchPattern pattern = compile_search_pattern(filter_text);

    _include_filters.push_back(pattern.raw_text);
    _include_filter_patterns.push_back(pattern);
    rebuild_visible_entries();
}

void ProcessedSources::add_exclude_filter(std::string filter_text)
{
    filter_text = trim_search_text(filter_text);
    if (filter_text.empty())
    {
        return;
    }

    const SearchPattern pattern = compile_search_pattern(filter_text);

    _exclude_filters.push_back(pattern.raw_text);
    _exclude_filter_patterns.push_back(pattern);
    rebuild_visible_entries();
}

void ProcessedSources::reset_filters()
{
    _include_filters.clear();
    _exclude_filters.clear();
    _include_filter_patterns.clear();
    _exclude_filter_patterns.clear();
    rebuild_visible_entries();
}

const std::vector<std::string>& ProcessedSources::include_filters() const
{
    return _include_filters;
}

const std::vector<std::string>& ProcessedSources::exclude_filters() const
{
    return _exclude_filters;
}

void ProcessedSources::hide_before_line_number(int line_number)
{
    _hidden_before_line_number = line_number > 1 ? std::optional<int>(line_number) : std::nullopt;
    rebuild_visible_entries();
}

std::optional<int> ProcessedSources::hidden_before_line_number() const
{
    return _hidden_before_line_number;
}

void ProcessedSources::hide_columns(int start_column, int end_column)
{
    if (start_column < 0 || end_column <= start_column)
    {
        _hidden_columns.reset();
        return;
    }

    _hidden_columns = HiddenColumnRange {start_column, end_column};
}

void ProcessedSources::reset_hidden_columns()
{
    _hidden_columns.reset();
}

std::optional<HiddenColumnRange> ProcessedSources::hidden_columns() const
{
    return _hidden_columns;
}

const ObservedLogLine& ProcessedSources::entry_at(AllLineIndex entry_index) const
{
    return _all_entries[entry_index];
}

std::optional<AllLineIndex> ProcessedSources::entry_index_for_visible_line(VisibleLineIndex visible_line_index) const
{
    if (visible_line_index.value < 0 || visible_line_index.value >= static_cast<int>(_visible_entry_indices.size()))
    {
        return std::nullopt;
    }

    return _visible_entry_indices[visible_line_index];
}

std::optional<VisibleLineIndex> ProcessedSources::visible_line_index_for_entry(AllLineIndex entry_index) const
{
    const auto visible_line = std::find(_visible_entry_indices.begin(), _visible_entry_indices.end(), entry_index);
    if (visible_line == _visible_entry_indices.end())
    {
        return std::nullopt;
    }

    return VisibleLineIndex {static_cast<int>(std::distance(_visible_entry_indices.begin(), visible_line))};
}

std::optional<int> ProcessedSources::line_number_for_visible_line(VisibleLineIndex visible_line_index) const
{
    if (visible_line_index.value < 0 || visible_line_index.value >= static_cast<int>(_visible_entry_indices.size()))
    {
        return std::nullopt;
    }

    return _visible_entry_indices[visible_line_index].value + 1;
}

std::optional<VisibleLineIndex> ProcessedSources::visible_line_index_for_line_number(int line_number) const
{
    if (line_number <= 0)
    {
        return std::nullopt;
    }

    return visible_line_index_for_entry(AllLineIndex {line_number - 1});
}

bool ProcessedSources::entry_index_is_visible(AllLineIndex entry_index) const
{
    return std::binary_search(_visible_entry_indices.begin(), _visible_entry_indices.end(), entry_index);
}

int ProcessedSources::line_count() const
{
    return static_cast<int>(_visible_entry_indices.size());
}

int ProcessedSources::total_line_count() const
{
    return static_cast<int>(_all_entries.size());
}

std::string ProcessedSources::rendered_line(int index) const
{
    const VisibleLineIndex visible_line_index {index};
    const AllLineIndex entry_index = _visible_entry_indices[visible_line_index];
    return render_entry(entry_index);
}

std::vector<std::string> ProcessedSources::rendered_lines(int first_index, int count) const
{
    if (count <= 0)
    {
        return {};
    }

    const int clamped_first = std::max(0, first_index);
    if (clamped_first >= static_cast<int>(_visible_entry_indices.size()))
    {
        return {};
    }

    const int last_index = std::min(static_cast<int>(_visible_entry_indices.size()), clamped_first + count);
    std::vector<std::string> lines;
    lines.reserve(static_cast<std::size_t>(last_index - clamped_first));
    for (int index = clamped_first; index < last_index; ++index)
    {
        const VisibleLineIndex visible_line_index {index};
        lines.push_back(render_entry(_visible_entry_indices[visible_line_index]));
    }

    return lines;
}

int ProcessedSources::max_rendered_line_width() const
{
    int width = 0;
    for (const auto entry_index : _visible_entry_indices)
    {
        width = std::max(width, static_cast<int>(render_entry(entry_index).size()));
    }

    return width;
}

std::string ProcessedSources::render_entry(AllLineIndex entry_index) const
{
    std::ostringstream output;
    const auto& entry = _all_entries[entry_index];
    output << entry_index.value + 1 << " ";
    if (!entry.parsed_time_text.empty())
    {
        output << "{" << entry.parsed_time_text << "} ";
    }

    if (_show_source_labels)
    {
        output << "[" << entry.source_label << "] ";
    }

    output << entry.text;
    return apply_hidden_columns(output.str());
}

void ProcessedSources::append_lines_immediately(const std::vector<ObservedLogLine>& lines)
{
    const AllLineIndex first_new_entry_index {static_cast<int>(_all_entries.size())};

    for (const auto& line : lines)
    {
        _all_entries.push_back(line);
    }

    expand_visible_entries(first_new_entry_index);
}

void ProcessedSources::flush_paused_updates()
{
    append_lines_immediately(_paused_updates);
    _paused_updates.clear();
}

void ProcessedSources::rebuild_visible_entries()
{
    _visible_entry_indices.clear();
    _visible_entry_indices.reserve(_all_entries.size());
    std::size_t index = 0;
    if (_hidden_before_line_number.has_value())
    {
        index = static_cast<std::size_t>(std::max(0, *_hidden_before_line_number - 1));
    }

    for (; index < _all_entries.size(); ++index)
    {
        const AllLineIndex entry_index {static_cast<int>(index)};
        if (entry_matches_filters(_all_entries[entry_index]))
        {
            _visible_entry_indices.push_back(entry_index);
        }
    }
}

void ProcessedSources::expand_visible_entries(AllLineIndex first_new_entry_index)
{
    int index = first_new_entry_index.value;
    if (_hidden_before_line_number.has_value())
    {
        index = std::max(index, std::max(0, *_hidden_before_line_number - 1));
    }

    for (; index < static_cast<int>(_all_entries.size()); ++index)
    {
        const AllLineIndex entry_index {index};
        if (entry_matches_filters(_all_entries[entry_index]))
        {
            _visible_entry_indices.push_back(entry_index);
        }
    }
}

bool ProcessedSources::entry_matches_filters(const ObservedLogLine& entry) const
{
    const std::string searchable_text = entry.source_label + "\n" + entry.text;
    const bool matches_include        = _include_filter_patterns.empty() || matches_any_pattern(searchable_text, _include_filter_patterns);
    const bool matches_exclude        = matches_any_pattern(searchable_text, _exclude_filter_patterns);
    return matches_include && !matches_exclude;
}

std::string ProcessedSources::apply_hidden_columns(std::string text) const
{
    if (!_hidden_columns.has_value())
    {
        return text;
    }

    const int clamped_start = std::clamp(_hidden_columns->start, 0, static_cast<int>(text.size()));
    const int clamped_end   = std::clamp(_hidden_columns->end, clamped_start, static_cast<int>(text.size()));
    if (clamped_start == clamped_end)
    {
        return text;
    }

    text.erase(static_cast<std::size_t>(clamped_start), static_cast<std::size_t>(clamped_end - clamped_start));
    return text;
}

} // namespace slayerlog
