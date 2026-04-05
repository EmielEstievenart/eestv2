#include "log_view_model.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <sstream>
#include <utility>

namespace slayerlog
{

void LogViewModel::append_lines(const std::vector<ObservedLogLine>& lines)
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

void LogViewModel::toggle_pause()
{
    _updates_paused = !_updates_paused;
    if (!_updates_paused)
    {
        flush_paused_updates();
    }
}

bool LogViewModel::updates_paused() const
{
    return _updates_paused;
}

void LogViewModel::set_show_source_labels(bool show_source_labels)
{
    _show_source_labels = show_source_labels;
}

void LogViewModel::add_include_filter(std::string filter_text)
{
    filter_text = trim_filter_text(filter_text);
    if (filter_text.empty())
    {
        return;
    }

    _include_filters.push_back(std::move(filter_text));
    rebuild_visible_entries();
    rebuild_find_matches();
}

void LogViewModel::add_exclude_filter(std::string filter_text)
{
    filter_text = trim_filter_text(filter_text);
    if (filter_text.empty())
    {
        return;
    }

    _exclude_filters.push_back(std::move(filter_text));
    rebuild_visible_entries();
    rebuild_find_matches();
}

void LogViewModel::reset_filters()
{
    _include_filters.clear();
    _exclude_filters.clear();
    rebuild_visible_entries();
    rebuild_find_matches();
}

const std::vector<std::string>& LogViewModel::include_filters() const
{
    return _include_filters;
}

const std::vector<std::string>& LogViewModel::exclude_filters() const
{
    return _exclude_filters;
}

void LogViewModel::hide_before_line_number(int line_number)
{
    _hidden_before_line_number = line_number > 1 ? std::optional<int>(line_number) : std::nullopt;
    rebuild_visible_entries();
    rebuild_find_matches();
}

std::optional<int> LogViewModel::hidden_before_line_number() const
{
    return _hidden_before_line_number;
}

bool LogViewModel::set_find_query(std::string query)
{
    query = trim_filter_text(query);
    if (query.empty())
    {
        clear_find_query();
        return false;
    }

    _find_query = std::move(query);
    rebuild_find_matches();
    return !_find_match_entry_indices.empty();
}

void LogViewModel::clear_find_query()
{
    _find_query.clear();
    _find_match_entry_indices.clear();
}

bool LogViewModel::find_active() const
{
    return !_find_query.empty();
}

const std::string& LogViewModel::find_query() const
{
    return _find_query;
}

int LogViewModel::total_find_match_count() const
{
    return static_cast<int>(_find_match_entry_indices.size());
}

int LogViewModel::visible_find_match_count() const
{
    return static_cast<int>(std::count_if(_find_match_entry_indices.begin(), _find_match_entry_indices.end(),
                                          [this](AllLineIndex entry_index) { return entry_index_is_visible(entry_index); }));
}

std::optional<AllLineIndex> LogViewModel::find_match_entry_index(FindResultIndex find_result_index) const
{
    if (find_result_index.value < 0 || find_result_index.value >= static_cast<int>(_find_match_entry_indices.size()))
    {
        return std::nullopt;
    }

    return _find_match_entry_indices[find_result_index];
}

std::optional<FindResultIndex> LogViewModel::find_match_position_for_entry_index(AllLineIndex entry_index) const
{
    const auto position = std::find(_find_match_entry_indices.begin(), _find_match_entry_indices.end(), entry_index);
    if (position == _find_match_entry_indices.end())
    {
        return std::nullopt;
    }

    return FindResultIndex {static_cast<int>(std::distance(_find_match_entry_indices.begin(), position))};
}

std::optional<VisibleLineIndex> LogViewModel::visible_line_index_for_entry(AllLineIndex entry_index) const
{
    const auto visible_line = std::find(_visible_entry_indices.begin(), _visible_entry_indices.end(), entry_index);
    if (visible_line == _visible_entry_indices.end())
    {
        return std::nullopt;
    }

    return VisibleLineIndex {static_cast<int>(std::distance(_visible_entry_indices.begin(), visible_line))};
}

std::optional<VisibleLineIndex> LogViewModel::visible_line_index_for_line_number(int line_number) const
{
    if (line_number <= 0)
    {
        return std::nullopt;
    }

    return visible_line_index_for_entry(AllLineIndex {line_number - 1});
}

bool LogViewModel::visible_line_matches_find(int visible_index) const
{
    if (visible_index < 0 || visible_index >= static_cast<int>(_visible_entry_indices.size()))
    {
        return false;
    }

    const VisibleLineIndex visible_line_index {visible_index};
    const AllLineIndex entry_index = _visible_entry_indices[visible_line_index];
    return std::binary_search(_find_match_entry_indices.begin(), _find_match_entry_indices.end(), entry_index);
}

bool LogViewModel::entry_index_is_visible(AllLineIndex entry_index) const
{
    return std::binary_search(_visible_entry_indices.begin(), _visible_entry_indices.end(), entry_index);
}

int LogViewModel::line_count() const
{
    return static_cast<int>(_visible_entry_indices.size());
}

int LogViewModel::total_line_count() const
{
    return static_cast<int>(_all_entries.size());
}

std::string LogViewModel::rendered_line(int index) const
{
    const VisibleLineIndex visible_line_index {index};
    const AllLineIndex entry_index = _visible_entry_indices[visible_line_index];
    return render_entry(entry_index);
}

std::vector<std::string> LogViewModel::rendered_lines(int first_index, int count) const
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

std::string LogViewModel::render_entry(AllLineIndex entry_index) const
{
    std::ostringstream output;
    const auto& entry = _all_entries[entry_index];
    output << entry_index.value + 1 << " ";
    if (_show_source_labels)
    {
        output << "[" << entry.source_label << "] ";
    }

    output << entry.text;
    return output.str();
}

void LogViewModel::append_lines_immediately(const std::vector<ObservedLogLine>& lines)
{
    const AllLineIndex first_new_entry_index {static_cast<int>(_all_entries.size())};

    for (const auto& line : lines)
    {
        _all_entries.push_back(line);
    }

    expand_visible_entries(first_new_entry_index);
    expand_find_matches(first_new_entry_index);
}

void LogViewModel::flush_paused_updates()
{
    append_lines_immediately(_paused_updates);
    _paused_updates.clear();
}

void LogViewModel::rebuild_visible_entries()
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

void LogViewModel::expand_visible_entries(AllLineIndex first_new_entry_index)
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

void LogViewModel::rebuild_find_matches()
{
    _find_match_entry_indices.clear();
    if (_find_query.empty())
    {
        return;
    }

    _find_match_entry_indices.reserve(_all_entries.size());
    for (std::size_t index = 0; index < _all_entries.size(); ++index)
    {
        const AllLineIndex entry_index {static_cast<int>(index)};
        if (entry_matches_find_query(_all_entries[entry_index]))
        {
            _find_match_entry_indices.push_back(entry_index);
        }
    }
}

void LogViewModel::expand_find_matches(AllLineIndex first_new_entry_index)
{
    if (_find_query.empty())
    {
        return;
    }

    for (int index = first_new_entry_index.value; index < static_cast<int>(_all_entries.size()); ++index)
    {
        const AllLineIndex entry_index {index};
        if (entry_matches_find_query(_all_entries[entry_index]))
        {
            _find_match_entry_indices.push_back(entry_index);
        }
    }
}

bool LogViewModel::entry_matches_find_query(const ObservedLogLine& entry) const
{
    return !_find_query.empty() && entry.text.find(_find_query) != std::string::npos;
}

bool LogViewModel::entry_matches_filters(const ObservedLogLine& entry) const
{
    const std::string searchable_text = entry.source_label + "\n" + entry.text;
    const bool matches_include        = _include_filters.empty() || matches_any_filter(searchable_text, _include_filters);
    const bool matches_exclude        = matches_any_filter(searchable_text, _exclude_filters);
    return matches_include && !matches_exclude;
}

bool LogViewModel::matches_any_filter(std::string_view haystack, const std::vector<std::string>& filters) const
{
    return std::any_of(filters.begin(), filters.end(),
                       [&](const std::string& filter) { return haystack.find(filter) != std::string_view::npos; });
}

std::string LogViewModel::trim_filter_text(std::string_view text)
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

} // namespace slayerlog
