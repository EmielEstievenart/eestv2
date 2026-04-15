#pragma once

#include <string>
#include <vector>

// Non-owning view over an externally-owned vector of strings.
// The caller owns the data and must ensure the referenced vector outlives the model.
class TextViewModel
{
public:
    // Point the model at an externally-owned vector.
    // The model does NOT take ownership -- the caller must keep the vector alive.
    void set_lines(const std::vector<std::string>& lines);

    [[nodiscard]] bool has_lines() const;
    [[nodiscard]] int line_count() const;
    [[nodiscard]] const std::string& line_at(int index) const;
    [[nodiscard]] int max_line_width() const;

private:
    const std::vector<std::string>* _lines = nullptr;
};
