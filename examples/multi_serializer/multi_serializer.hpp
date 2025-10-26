#pragma once

#include <iostream>
#include <stdexcept>
#include <tuple>

// Helper template to find the index of a type in a parameter pack
template <typename T, typename... Types>
struct TypeIndex;

// Base case: type found at position 0
template <typename T, typename... Rest>
struct TypeIndex<T, T, Rest...>
{
    static constexpr std::size_t value = 1;
};

// Recursive case: type not found yet, check next position
template <typename T, typename First, typename... Rest>
struct TypeIndex<T, First, Rest...>
{
    static constexpr std::size_t value = 1 + TypeIndex<T, Rest...>::value;
};

// Main variadic template class
template <typename... Types>
class MultiSerializer
{
public:
    // Print function that takes any of the types from the parameter pack
    template <typename T>
    void print() const
    {
        std::cout << "Type corresponds to position: " << TypeIndex<T, Types...>::value << "\n";
    }

    // Alternative: print with an instance of the type
    template <typename T>
    void print(const T& /* instance */) const
    {
        std::cout << "Type corresponds to position: " << TypeIndex<T, Types...>::value << "\n";
    }

    /**
     * @brief Calls the print function of the class at the specified index
     * @param index The index of the class (1-based) to call print on
     * @throws std::out_of_range if index is invalid
     */
    void print_at_index(std::size_t index) const
    {
        if (index < 1 || index > sizeof...(Types))
        {
            throw std::out_of_range("Index out of range");
        }
        _print_at_index_impl(index, std::index_sequence_for<Types...> {});
    }

private:
    template <std::size_t... Indices>
    void _print_at_index_impl(std::size_t index, std::index_sequence<Indices...>) const
    {
        // Use fold expression with comma operator to iterate through indices
        ((index == Indices + 1 ? (std::tuple_element_t<Indices, std::tuple<Types...>>::print(), true) : false) || ...);
    }
};
