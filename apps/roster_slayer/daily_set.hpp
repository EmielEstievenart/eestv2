#pragma once

#include "shift.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace roster_slayer
{
class DailySet
{
public:
    explicit DailySet(std::vector<Shift> desired_shifts);

    void set(std::size_t index, const Shift& shift);

    void clear(std::size_t index);

    [[nodiscard]] bool is_set(std::size_t index) const;

    [[nodiscard]] const Shift& get(std::size_t index) const;

    [[nodiscard]] std::size_t size() const;

    [[nodiscard]] std::size_t nr_of_assigned_entries() const;

    [[nodiscard]] std::uint64_t get_nr_of_combinations() const;

    [[nodiscard]] DailySet get_set(std::uint64_t combination_index) const;

private:
    using CountMap = std::unordered_map<std::string, std::size_t>;
    using ShiftMap = std::unordered_map<std::string, Shift>;

    static std::uint64_t factorial(std::size_t n);

    static std::uint64_t multinomial_count(const std::vector<std::size_t>& counts);

    static std::string shift_id(const Shift& shift);

    void validate_index(std::size_t index) const;

    void validate_current_state_against_desired() const;

    [[nodiscard]] CountMap get_remaining_shift_count_map() const;

    [[nodiscard]] std::vector<std::size_t> get_remaining_shift_counts() const;

    [[nodiscard]] std::vector<std::string> get_shift_ids_in_order() const;

    [[nodiscard]] ShiftMap get_shift_lookup() const;

    [[nodiscard]] std::vector<std::size_t> get_empty_positions() const;

    std::vector<Shift> _desired_shifts;
    std::vector<std::optional<Shift>> _assigned_shifts;
};
} // namespace roster_slayer
