#pragma once

#include "days_of_the_week.hpp"
#include "one_day_planning.hpp"
#include "shift.hpp"
#include "weekday_shifts.hpp"
#include "weekend_shifts.hpp"

#include <magic_enum/magic_enum.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

struct WeekPlanning
{
    void print(std::ostream& out) const
    {
        constexpr std::array<std::string_view, 8> headers {"person", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"};
        constexpr std::array<std::size_t, 8> column_widths {6, 12, 12, 12, 12, 12, 12, 12};

        auto print_separator = [&out, &column_widths]()
        {
            out << '+';
            for (const auto width : column_widths)
            {
                for (std::size_t index = 0; index < width + 2; ++index)
                {
                    out << '-';
                }
                out << '+';
            }
            out << '\n';
        };

        auto print_cell = [&out](std::string_view value, std::size_t width)
        {
            out << ' ' << value;
            for (std::size_t index = value.size(); index < width; ++index)
            {
                out << ' ';
            }
            out << " |";
        };

        print_separator();
        out << '|';
        for (std::size_t column = 0; column < headers.size(); ++column)
        {
            print_cell(headers[column], column_widths[column]);
        }
        out << '\n';
        print_separator();

        std::size_t nr_of_persons = 0;
        if (monday.has_value())
        {
            nr_of_persons = monday->size();
        }
        else if (tuesday.has_value())
        {
            nr_of_persons = tuesday->size();
        }
        else if (wednesday.has_value())
        {
            nr_of_persons = wednesday->size();
        }
        else if (thursday.has_value())
        {
            nr_of_persons = thursday->size();
        }
        else if (friday.has_value())
        {
            nr_of_persons = friday->size();
        }
        else if (saturday.has_value())
        {
            nr_of_persons = saturday->size();
        }
        else if (sunday.has_value())
        {
            nr_of_persons = sunday->size();
        }

        for (std::size_t person = 0; person < nr_of_persons; ++person)
        {
            const auto person_label = std::to_string(person);

            out << '|';
            print_cell(person_label, column_widths[0]);

            if (monday.has_value() && monday->is_set(person))
            {
                print_cell(magic_enum::enum_name(monday->get(person).get_code()), column_widths[1]);
            }
            else
            {
                print_cell("UNSET", column_widths[1]);
            }

            if (tuesday.has_value() && tuesday->is_set(person))
            {
                print_cell(magic_enum::enum_name(tuesday->get(person).get_code()), column_widths[2]);
            }
            else
            {
                print_cell("UNSET", column_widths[2]);
            }

            if (wednesday.has_value() && wednesday->is_set(person))
            {
                print_cell(magic_enum::enum_name(wednesday->get(person).get_code()), column_widths[3]);
            }
            else
            {
                print_cell("UNSET", column_widths[3]);
            }

            if (thursday.has_value() && thursday->is_set(person))
            {
                print_cell(magic_enum::enum_name(thursday->get(person).get_code()), column_widths[4]);
            }
            else
            {
                print_cell("UNSET", column_widths[4]);
            }

            if (friday.has_value() && friday->is_set(person))
            {
                print_cell(magic_enum::enum_name(friday->get(person).get_code()), column_widths[5]);
            }
            else
            {
                print_cell("UNSET", column_widths[5]);
            }

            if (saturday.has_value() && saturday->is_set(person))
            {
                print_cell(magic_enum::enum_name(saturday->get(person).get_code()), column_widths[6]);
            }
            else
            {
                print_cell("UNSET", column_widths[6]);
            }

            if (sunday.has_value() && sunday->is_set(person))
            {
                print_cell(magic_enum::enum_name(sunday->get(person).get_code()), column_widths[7]);
            }
            else
            {
                print_cell("UNSET", column_widths[7]);
            }

            out << '\n';
        }

        print_separator();
    }

    std::optional<ShiftType> getPreviousShiftType(int person, DaysOfTheWeek day) const
    {
        auto get_if_optional_set = [](const auto& optional_planning, std::size_t person_index) -> std::optional<ShiftType>
        {
            if (!optional_planning.has_value() || person_index >= optional_planning->size() || !optional_planning->is_set(person_index))
            {
                return std::nullopt;
            }

            return optional_planning->get(person_index)._type;
        };

        if (person < 0)
        {
            return std::nullopt;
        }

        const auto person_index = static_cast<std::size_t>(person);

        switch (day)
        {
        case DaysOfTheWeek::monday:
        {
            // Monday looks back to Sunday of the previous roster week.
            // Because rows rotate, person 0 came from the final row, person 1 from row 0, etc.
            if (!sunday.has_value() || person_index >= sunday->size())
            {
                return std::nullopt;
            }

            const auto previous_person = person_index == 0 ? sunday->size() - 1 : person_index - 1;
            return get_if_optional_set(sunday, previous_person);
        }

        case DaysOfTheWeek::tuesday:
            return get_if_optional_set(monday, person_index);

        case DaysOfTheWeek::wednesday:
            return get_if_optional_set(tuesday, person_index);

        case DaysOfTheWeek::thursday:
            return get_if_optional_set(wednesday, person_index);

        case DaysOfTheWeek::friday:
            return get_if_optional_set(thursday, person_index);

        case DaysOfTheWeek::saturday:
            return get_if_optional_set(friday, person_index);

        case DaysOfTheWeek::sunday:
            return get_if_optional_set(saturday, person_index);
        }

        return std::nullopt;
    }

    std::optional<WeekdayShiftCode> getPreviousWeekdayShift(int person, DaysOfTheWeek day) const
    {
        auto get_if_optional_set = [](const std::optional<OneDayPlanning<WeekdayShiftCode>>& optional_planning,
                                      std::size_t person_index) -> std::optional<WeekdayShiftCode>
        {
            if (!optional_planning.has_value() || person_index >= optional_planning->size() || !optional_planning->is_set(person_index))
            {
                return std::nullopt;
            }

            return optional_planning->get(person_index).get_code();
        };

        if (person < 0)
        {
            return std::nullopt;
        }

        const auto person_index = static_cast<std::size_t>(person);

        switch (day)
        {
        case DaysOfTheWeek::tuesday:
            return get_if_optional_set(monday, person_index);

        case DaysOfTheWeek::wednesday:
            return get_if_optional_set(tuesday, person_index);

        case DaysOfTheWeek::thursday:
            return get_if_optional_set(wednesday, person_index);

        case DaysOfTheWeek::friday:
            return get_if_optional_set(thursday, person_index);

        case DaysOfTheWeek::saturday:
            return get_if_optional_set(friday, person_index);

        case DaysOfTheWeek::monday:
        case DaysOfTheWeek::sunday:
            return std::nullopt;
        }

        return std::nullopt;
    }

    std::optional<WeekendShiftCode> getPreviousWeekendShift(int person, DaysOfTheWeek day) const
    {
        auto get_if_optional_set = [](const std::optional<OneDayPlanning<WeekendShiftCode>>& optional_planning,
                                      std::size_t person_index) -> std::optional<WeekendShiftCode>
        {
            if (!optional_planning.has_value() || person_index >= optional_planning->size() || !optional_planning->is_set(person_index))
            {
                return std::nullopt;
            }

            return optional_planning->get(person_index).get_code();
        };

        if (person < 0)
        {
            return std::nullopt;
        }

        const auto person_index = static_cast<std::size_t>(person);

        switch (day)
        {
        case DaysOfTheWeek::monday:
        {
            // Monday looks back to Sunday of the previous roster week.
            // Because rows rotate, person 0 came from the final row, person 1 from row 0, etc.
            if (!sunday.has_value() || person_index >= sunday->size())
            {
                return std::nullopt;
            }

            const auto previous_person = person_index == 0 ? sunday->size() - 1 : person_index - 1;
            return get_if_optional_set(sunday, previous_person);
        }

        case DaysOfTheWeek::sunday:
            return get_if_optional_set(saturday, person_index);

        case DaysOfTheWeek::tuesday:
        case DaysOfTheWeek::wednesday:
        case DaysOfTheWeek::thursday:
        case DaysOfTheWeek::friday:
        case DaysOfTheWeek::saturday:
            return std::nullopt;
        }

        return std::nullopt;
    }

    std::optional<OneDayPlanning<WeekendShiftCode>> saturday;
    std::optional<OneDayPlanning<WeekendShiftCode>> sunday;
    std::optional<OneDayPlanning<WeekdayShiftCode>> monday;
    std::optional<OneDayPlanning<WeekdayShiftCode>> tuesday;
    std::optional<OneDayPlanning<WeekdayShiftCode>> wednesday;
    std::optional<OneDayPlanning<WeekdayShiftCode>> thursday;
    std::optional<OneDayPlanning<WeekdayShiftCode>> friday;
};
