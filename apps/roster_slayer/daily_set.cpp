#include "daily_set.hpp"

#include <stdexcept>
#include <utility>

namespace roster_slayer
{
DailySet::DailySet(std::vector<Shift> desired_shifts)
    : _desired_shifts{std::move(desired_shifts)},
      _assigned_shifts(_desired_shifts.size())
{
}

void DailySet::set(std::size_t index, const Shift& shift)
{
    validate_index(index);

    const auto old_value = _assigned_shifts[index];
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

void DailySet::clear(std::size_t index)
{
    validate_index(index);
    _assigned_shifts[index].reset();
}

bool DailySet::is_set(std::size_t index) const
{
    validate_index(index);
    return _assigned_shifts[index].has_value();
}

const Shift& DailySet::get(std::size_t index) const
{
    validate_index(index);

    if (!_assigned_shifts[index].has_value())
    {
        throw std::logic_error("DailySet entry is not set");
    }

    return *_assigned_shifts[index];
}

std::size_t DailySet::size() const
{
    return _assigned_shifts.size();
}

std::size_t DailySet::nr_of_assigned_entries() const
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

std::uint64_t DailySet::get_nr_of_combinations() const
{
    const auto remaining_counts = get_remaining_shift_counts();
    return multinomial_count(remaining_counts);
}

std::uint64_t DailySet::factorial(std::size_t n)
{
    std::uint64_t result = 1;
    for (std::size_t i = 2; i <= n; ++i)
    {
        result *= static_cast<std::uint64_t>(i);
    }

    return result;
}

std::uint64_t DailySet::multinomial_count(const std::vector<std::size_t>& counts)
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

std::string DailySet::shift_id(const Shift& shift)
{
    return shift.get_code();
}

void DailySet::validate_index(std::size_t index) const
{
    if (index >= _assigned_shifts.size())
    {
        throw std::out_of_range("DailySet index is outside the configured size");
    }
}

void DailySet::validate_current_state_against_desired() const
{
    (void)get_remaining_shift_counts();
}

std::vector<std::size_t> DailySet::get_remaining_shift_counts() const
{
    if (_desired_shifts.size() != _assigned_shifts.size())
    {
        throw std::logic_error("Desired shift count does not match DailySet size");
    }

    CountMap remaining_counts;
    for (const auto& shift : _desired_shifts)
    {
        ++remaining_counts[shift_id(shift)];
    }

    std::size_t nr_of_empty_entries = 0;

    for (const auto& assigned : _assigned_shifts)
    {
        if (!assigned.has_value())
        {
            ++nr_of_empty_entries;
            continue;
        }

        const auto key = shift_id(*assigned);
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

    std::vector<std::size_t> counts;
    counts.reserve(remaining_counts.size());

    std::size_t total_remaining = 0;
    for (const auto& [key, count] : remaining_counts)
    {
        (void)key;
        counts.push_back(count);
        total_remaining += count;
    }

    if (total_remaining != nr_of_empty_entries)
    {
        throw std::logic_error("Internal mismatch between empty entries and remaining desired shifts");
    }

    return counts;
}

} // namespace roster_slayer
