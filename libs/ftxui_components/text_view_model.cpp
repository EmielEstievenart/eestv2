#include <ftxui_components/text_view_model.hpp>

#include <algorithm>
#include <stdexcept>

void TextViewModel::set_lines(const std::vector<std::string>& lines)
{
    _lines = &lines;
}

bool TextViewModel::has_lines() const
{
    return _lines != nullptr;
}

int TextViewModel::line_count() const
{
    if (_lines == nullptr)
    {
        return 0;
    }
    return static_cast<int>(_lines->size());
}

const std::string& TextViewModel::line_at(int index) const
{
    if (_lines == nullptr || index < 0 || index >= line_count())
    {
        throw std::out_of_range("TextViewModel::line_at index out of range");
    }

    return (*_lines)[static_cast<std::size_t>(index)];
}

int TextViewModel::max_line_width() const
{
    if (_lines == nullptr)
    {
        return 0;
    }

    int max_width = 0;
    for (const auto& line : *_lines)
    {
        max_width = std::max(max_width, static_cast<int>(line.size()));
    }
    return max_width;
}
