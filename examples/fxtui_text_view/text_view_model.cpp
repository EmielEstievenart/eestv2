#include "text_view_model.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

void TextViewModel::append_line(std::string line)
{
    _lines.push_back(std::move(line));
}

void TextViewModel::append_lines(const std::vector<std::string>& lines)
{
    _lines.insert(_lines.end(), lines.begin(), lines.end());
}

int TextViewModel::line_count() const
{
    return static_cast<int>(_lines.size());
}

const std::string& TextViewModel::line_at(int index) const
{
    if (index < 0 || index >= line_count())
    {
        throw std::out_of_range("TextViewModel::line_at index out of range");
    }

    return _lines[static_cast<std::size_t>(index)];
}

int TextViewModel::max_line_width() const
{
    int max_width = 0;
    for (const auto& line : _lines)
    {
        max_width = std::max(max_width, static_cast<int>(line.size()));
    }
    return max_width;
}
