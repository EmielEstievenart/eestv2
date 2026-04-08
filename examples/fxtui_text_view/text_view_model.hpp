#pragma once

#include <string>
#include <vector>

class TextViewModel
{
public:
    void append_line(std::string line);
    void append_lines(const std::vector<std::string>& lines);

    [[nodiscard]] int line_count() const;
    [[nodiscard]] const std::string& line_at(int index) const;
    [[nodiscard]] int max_line_width() const;

private:
    std::vector<std::string> _lines;
};
