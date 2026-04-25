#pragma once

#include "shift.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

inline std::uint64_t factorial(std::size_t n)
{
    std::uint64_t result = 1;
    for (std::size_t i = 2; i <= n; ++i)
    {
        result *= static_cast<std::uint64_t>(i);
    }

    return result;
}

inline std::uint64_t multinomial_count(const std::vector<std::size_t>& counts)
{
    std::size_t total = 0;
    for (const auto count : counts)
    {
        total += count;
    }

    std::uint64_t result = factorial(total);
    for (const auto count : counts)
    {
        result /= factorial(count);
    }

    return result;
}

template <typename shiftEnum>
class OneDayPlanning
{
public:
    explicit OneDayPlanning(std::vector<Shift<shiftEnum>> desired_shifts) : _desired_shifts {std::move(desired_shifts)}, _assigned_shifts(_desired_shifts.size()) { }

    void set(std::size_t index, const Shift<shiftEnum>& shift)
    {
        validate_index(index);

        const auto old_value    = _assigned_shifts[index];
        _assigned_shifts[index] = shift;

        try
        {
            validate_current_state_against_desired();
        }
        catch (...)
        {
            _assigned_shifts[index] = old_value;
            throw;
        }
    }

    void clear(std::size_t index)
    {
        validate_index(index);
        _assigned_shifts[index].reset();
    }

    bool is_set(std::size_t index) const
    {
        validate_index(index);
        return _assigned_shifts[index].has_value();
    }

    const Shift<shiftEnum>& get(std::size_t index) const
    {
        validate_index(index);

        if (!_assigned_shifts[index].has_value())
        {
            throw std::logic_error("WeekdayDailySet entry is not set");
        }

        return *_assigned_shifts[index];
    }

    std::size_t size() const { return _assigned_shifts.size(); }

    std::size_t nr_of_assigned_entries() const
    {
        std::size_t count = 0;
        for (const auto& entry : _assigned_shifts)
        {
            if (entry.has_value())
            {
                ++count;
            }
        }

        return count;
    }

    std::uint64_t get_nr_of_combinations() const
    {
        const auto remaining_counts = get_remaining_shift_counts();
        return multinomial_count(remaining_counts);
    }

    Shift<shiftEnum> get_set(std::uint64_t combination_index) const
    {
        validate_current_state_against_desired();

        WeekdayDailySet result = *this;

        CountMap remaining_counts     = get_remaining_shift_count_map();
        const auto shift_ids_in_order = get_shift_ids_in_order();
        const auto shift_lookup       = get_shift_lookup();
        const auto empty_positions    = get_empty_positions();

        const std::uint64_t total_combinations = multinomial_count(get_remaining_shift_counts());
        if (combination_index >= total_combinations)
        {
            throw std::out_of_range("WeekdayDailySet::get_set(): combination index out of range");
        }

        for (const std::size_t pos : empty_positions)
        {
            bool chosen = false;

            for (const auto& shift_id_value : shift_ids_in_order)
            {
                auto iter = remaining_counts.find(shift_id_value);
                if (iter == remaining_counts.end() || iter->second == 0)
                {
                    continue;
                }

                --iter->second;

                std::vector<std::size_t> counts;
                counts.reserve(remaining_counts.size());
                for (const auto& [key, count] : remaining_counts)
                {
                    (void)key;
                    counts.push_back(count);
                }

                const std::uint64_t block_size = daily_set_detail::multinomial_count(counts);

                if (combination_index < block_size)
                {
                    result._assigned_shifts[pos] = shift_lookup.at(shift_id_value);
                    chosen                       = true;
                    break;
                }

                combination_index -= block_size;
                ++iter->second;
            }

            if (!chosen)
            {
                throw std::logic_error("WeekdayDailySet::get_set(): failed to construct requested combination");
            }
        }

        result.validate_current_state_against_desired();
        return result;
    }

    void print(std::ostream& out) const
    {
        out << '[';
        for (std::size_t i = 0; i < _assigned_shifts.size(); ++i)
        {
            if (i > 0)
            {
                out << ' ';
            }

            if (_assigned_shifts[i])
            {
                out << _assigned_shifts[i]->get_code();
            }
            else
            {
                out << "UNSET";
            }
        }
        out << ']';
    }

private:
    using CountMap = std::unordered_map<std::uint16_t, std::size_t>;
    using ShiftMap = std::unordered_map<std::uint16_t, Shift<shiftEnum>>;

    void validate_index(std::size_t index) const
    {
        if (index >= _assigned_shifts.size())
        {
            throw std::out_of_range("WeekdayDailySet index is outside the configured size");
        }
    }

    void validate_current_state_against_desired() const { (void)get_remaining_shift_counts(); }

    CountMap get_remaining_shift_count_map() const
    {
        if (_desired_shifts.size() != _assigned_shifts.size())
        {
            throw std::logic_error("Desired shift count does not match WeekdayDailySet size");
        }

        CountMap remaining_counts;
        for (const auto& shift : _desired_shifts)
        {
            ++remaining_counts[shift.shift_id()];
        }

        std::size_t nr_of_empty_entries = 0;

        for (const auto& assigned : _assigned_shifts)
        {
            if (!assigned.has_value())
            {
                ++nr_of_empty_entries;
                continue;
            }

            const auto key  = assigned->shift_id();
            const auto iter = remaining_counts.find(key);
            if (iter == remaining_counts.end())
            {
                throw std::invalid_argument("Assigned shift is not part of the desired shift pool");
            }

            if (iter->second == 0)
            {
                throw std::invalid_argument("Assigned shift is used more often than allowed");
            }

            --iter->second;
        }

        std::size_t total_remaining = 0;
        for (const auto& [key, count] : remaining_counts)
        {
            (void)key;
            total_remaining += count;
        }

        if (total_remaining != nr_of_empty_entries)
        {
            throw std::logic_error("Internal mismatch between empty entries and remaining desired shifts");
        }

        return remaining_counts;
    }

    std::vector<std::size_t> get_remaining_shift_counts() const
    {
        const auto remaining_counts = get_remaining_shift_count_map();

        std::vector<std::size_t> counts;
        counts.reserve(remaining_counts.size());

        for (const auto& [key, count] : remaining_counts)
        {
            (void)key;
            counts.push_back(count);
        }

        return counts;
    }

    std::vector<std::uint16_t> get_shift_ids_in_order() const
    {
        std::vector<std::uint16_t> ids;

        for (const auto& shift : _desired_shifts)
        {
            const auto id           = shift.shift_id();
            const auto already_seen = std::find(ids.begin(), ids.end(), id) != ids.end();
            if (!already_seen)
            {
                ids.push_back(id);
            }
        }

        return ids;
    }

    ShiftMap get_shift_lookup() const
    {
        ShiftMap lookup;

        for (const auto& shift : _desired_shifts)
        {
            lookup.emplace(shift.shift_id(), shift);
        }

        return lookup;
    }

    std::vector<std::size_t> get_empty_positions() const
    {
        std::vector<std::size_t> positions;

        for (std::size_t i = 0; i < _assigned_shifts.size(); ++i)
        {
            if (!_assigned_shifts[i].has_value())
            {
                positions.push_back(i);
            }
        }

        return positions;
    }

    std::vector<Shift<shiftEnum>> _desired_shifts;
    std::vector<std::optional<Shift<shiftEnum>>> _assigned_shifts;
};