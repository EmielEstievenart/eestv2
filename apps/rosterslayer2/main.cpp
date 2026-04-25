#include "days_of_the_week.hpp"
#include "planning_master.hpp"
#include "weekday_shifts.hpp"
#include "weekend_shifts.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <magic_enum/magic_enum.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <exception>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace
{

constexpr std::size_t nr_of_persons = 10;
constexpr std::size_t nr_of_days    = 7;
constexpr int cell_width            = 14;
const std::string output_file_path  = "roster_slayer_results.txt";

using RosterGrid = std::array<std::array<int, nr_of_days>, nr_of_persons>;

template <typename Enum>
std::vector<std::string> enum_labels()
{
    std::vector<std::string> labels;
    for (std::string_view name : magic_enum::enum_names<Enum>())
    {
        labels.emplace_back(name);
    }

    return labels;
}

template <typename Enum>
std::vector<std::string> optional_enum_labels()
{
    auto labels = enum_labels<Enum>();
    labels.insert(labels.begin(), "X");

    return labels;
}

const std::vector<std::string> day_labels           = enum_labels<DaysOfTheWeek>();
const std::vector<std::string> weekday_shift_labels = optional_enum_labels<WeekdayShiftCode>();
const std::vector<std::string> weekend_shift_labels = optional_enum_labels<WeekendShiftCode>();

const std::array<DaysOfTheWeek, nr_of_days> days {
    DaysOfTheWeek::monday,
    DaysOfTheWeek::tuesday,
    DaysOfTheWeek::wednesday,
    DaysOfTheWeek::thursday,
    DaysOfTheWeek::friday,
    DaysOfTheWeek::saturday,
    DaysOfTheWeek::sunday,
};

bool is_weekend(std::size_t day_index)
{
    return day_index == 5 || day_index == 6;
}

bool is_day_in_search_range(std::size_t day_index, int start_day_index, int stop_day_index)
{
    const auto start = static_cast<std::size_t>(start_day_index);
    const auto stop  = static_cast<std::size_t>(stop_day_index);

    if (start <= stop)
    {
        return day_index >= start && day_index <= stop;
    }

    return day_index >= start || day_index <= stop;
}

std::string padded_label(const std::string& label)
{
    if (label.size() >= static_cast<std::size_t>(cell_width - 4))
    {
        return label;
    }

    return label + std::string(static_cast<std::size_t>(cell_width - 4) - label.size(), ' ');
}

std::string cell_label(const RosterGrid& grid, std::size_t person, std::size_t day)
{
    const auto& labels = is_weekend(day) ? weekend_shift_labels : weekday_shift_labels;
    return padded_label(labels[static_cast<std::size_t>(grid[person][day])]);
}

void refresh_cell_labels(const RosterGrid& grid, std::array<std::array<std::string, nr_of_days>, nr_of_persons>& labels)
{
    for (std::size_t person = 0; person < nr_of_persons; ++person)
    {
        for (std::size_t day = 0; day < nr_of_days; ++day)
        {
            labels[person][day] = cell_label(grid, person, day);
        }
    }
}

WeekdayShift weekday_shift_from_cell(int cell)
{
    switch (cell)
    {
    case 1:
        return get_off_shift();
    case 2:
        return get_early_shift_a();
    case 3:
        return get_early_shift_b();
    case 4:
        return get_day_shift();
    case 5:
        return get_late_shift_1();
    case 6:
        return get_late_shift_2();
    default:
        throw std::logic_error("Unknown weekday shift selection");
    }
}

WeekendShift weekend_shift_from_cell(int cell)
{
    switch (cell)
    {
    case 1:
        return get_weekend_off_shift();
    case 2:
        return get_weekend_early_shift_a();
    case 3:
        return get_weekend_early_shift_b();
    case 4:
        return get_weekend_split_day_shift();
    case 5:
        return get_weekend_split_late_shift();
    case 6:
        return get_weekend_late_shift();
    default:
        throw std::logic_error("Unknown weekend shift selection");
    }
}

int weekday_cell_from_shift(WeekdayShiftCode code)
{
    const auto index = magic_enum::enum_index(code);
    if (!index.has_value())
    {
        throw std::logic_error("Unknown weekday shift code");
    }

    return static_cast<int>(*index) + 1;
}

int weekend_cell_from_shift(WeekendShiftCode code)
{
    const auto index = magic_enum::enum_index(code);
    if (!index.has_value())
    {
        throw std::logic_error("Unknown weekend shift code");
    }

    return static_cast<int>(*index) + 1;
}

bool has_locked_weekday_cells(const RosterGrid& grid, std::size_t day)
{
    for (std::size_t person = 0; person < nr_of_persons; ++person)
    {
        if (grid[person][day] != 0)
        {
            return true;
        }
    }

    return false;
}

bool has_locked_weekend_cells(const RosterGrid& grid, std::size_t day)
{
    for (std::size_t person = 0; person < nr_of_persons; ++person)
    {
        if (grid[person][day] != 0)
        {
            return true;
        }
    }

    return false;
}

OneDayPlanning<WeekdayShiftCode> build_weekday_planning(const RosterGrid& grid, std::size_t day)
{
    OneDayPlanning<WeekdayShiftCode> planning(get_weekday_required_shifts());
    for (std::size_t person = 0; person < nr_of_persons; ++person)
    {
        if (grid[person][day] != 0)
        {
            planning.set(person, weekday_shift_from_cell(grid[person][day]));
        }
    }

    return planning;
}

OneDayPlanning<WeekendShiftCode> build_weekend_planning(const RosterGrid& grid, std::size_t day)
{
    OneDayPlanning<WeekendShiftCode> planning(get_weekend_required_shifts());
    for (std::size_t person = 0; person < nr_of_persons; ++person)
    {
        if (grid[person][day] != 0)
        {
            planning.set(person, weekend_shift_from_cell(grid[person][day]));
        }
    }

    return planning;
}

WeekPlanning build_week_planning(const RosterGrid& grid, int start_day_index, int stop_day_index)
{
    WeekPlanning planning;

    if (is_day_in_search_range(0, start_day_index, stop_day_index) && has_locked_weekday_cells(grid, 0))
    {
        planning.monday.emplace(build_weekday_planning(grid, 0));
    }
    if (is_day_in_search_range(1, start_day_index, stop_day_index) && has_locked_weekday_cells(grid, 1))
    {
        planning.tuesday.emplace(build_weekday_planning(grid, 1));
    }
    if (is_day_in_search_range(2, start_day_index, stop_day_index) && has_locked_weekday_cells(grid, 2))
    {
        planning.wednesday.emplace(build_weekday_planning(grid, 2));
    }
    if (is_day_in_search_range(3, start_day_index, stop_day_index) && has_locked_weekday_cells(grid, 3))
    {
        planning.thursday.emplace(build_weekday_planning(grid, 3));
    }
    if (is_day_in_search_range(4, start_day_index, stop_day_index) && has_locked_weekday_cells(grid, 4))
    {
        planning.friday.emplace(build_weekday_planning(grid, 4));
    }
    if (is_day_in_search_range(5, start_day_index, stop_day_index) && has_locked_weekend_cells(grid, 5))
    {
        planning.saturday.emplace(build_weekend_planning(grid, 5));
    }
    if (is_day_in_search_range(6, start_day_index, stop_day_index) && has_locked_weekend_cells(grid, 6))
    {
        planning.sunday.emplace(build_weekend_planning(grid, 6));
    }

    return planning;
}

WeekPlanning build_default_week_planning()
{
    WeekPlanning planning;

    planning.saturday.emplace(get_weekend_required_shifts());
    planning.saturday->set(0, get_weekend_off_shift());
    planning.saturday->set(2, get_weekend_off_shift());
    planning.saturday->set(4, get_weekend_off_shift());
    planning.saturday->set(6, get_weekend_off_shift());
    planning.saturday->set(8, get_weekend_off_shift());

    planning.sunday.emplace(get_weekend_required_shifts());
    planning.sunday->set(0, get_weekend_off_shift());
    planning.sunday->set(2, get_weekend_off_shift());
    planning.sunday->set(4, get_weekend_off_shift());
    planning.sunday->set(6, get_weekend_off_shift());
    planning.sunday->set(8, get_weekend_off_shift());

    return planning;
}

void apply_weekday_planning_to_grid(const OneDayPlanning<WeekdayShiftCode>& planning, RosterGrid& grid, std::size_t day)
{
    for (std::size_t person = 0; person < nr_of_persons && person < planning.size(); ++person)
    {
        grid[person][day] = planning.is_set(person) ? weekday_cell_from_shift(planning.get(person).get_code()) : 0;
    }
}

void apply_weekend_planning_to_grid(const OneDayPlanning<WeekendShiftCode>& planning, RosterGrid& grid, std::size_t day)
{
    for (std::size_t person = 0; person < nr_of_persons && person < planning.size(); ++person)
    {
        grid[person][day] = planning.is_set(person) ? weekend_cell_from_shift(planning.get(person).get_code()) : 0;
    }
}

void apply_week_planning_to_grid(const WeekPlanning& planning, RosterGrid& grid)
{
    if (planning.monday.has_value())
    {
        apply_weekday_planning_to_grid(*planning.monday, grid, 0);
    }
    if (planning.tuesday.has_value())
    {
        apply_weekday_planning_to_grid(*planning.tuesday, grid, 1);
    }
    if (planning.wednesday.has_value())
    {
        apply_weekday_planning_to_grid(*planning.wednesday, grid, 2);
    }
    if (planning.thursday.has_value())
    {
        apply_weekday_planning_to_grid(*planning.thursday, grid, 3);
    }
    if (planning.friday.has_value())
    {
        apply_weekday_planning_to_grid(*planning.friday, grid, 4);
    }
    if (planning.saturday.has_value())
    {
        apply_weekend_planning_to_grid(*planning.saturday, grid, 5);
    }
    if (planning.sunday.has_value())
    {
        apply_weekend_planning_to_grid(*planning.sunday, grid, 6);
    }
}

ftxui::Element render_status(const std::string& status_message, bool status_is_error, bool search_running, std::size_t found_count)
{
    if (search_running)
    {
        return ftxui::text("Searching... editing is disabled until the search finishes.") | ftxui::color(ftxui::Color::Yellow);
    }

    if (status_is_error)
    {
        return ftxui::text(status_message) | ftxui::color(ftxui::Color::Red);
    }

    if (!status_message.empty())
    {
        return ftxui::text(status_message) | ftxui::color(ftxui::Color::GreenLight);
    }

    return ftxui::text("Found results: " + std::to_string(found_count));
}

}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    RosterGrid grid {};
    apply_week_planning_to_grid(build_default_week_planning(), grid);

    std::array<std::array<std::string, nr_of_days>, nr_of_persons> cell_labels;
    refresh_cell_labels(grid, cell_labels);

    auto screen = ftxui::ScreenInteractive::Fullscreen();

    int start_day_index = 0;
    int stop_day_index  = 6;

    std::mutex state_mutex;
    std::atomic<bool> search_running {false};
    std::size_t found_count = 0;
    std::string status_message;
    bool status_is_error = false;
    std::thread search_thread;

    auto set_status = [&](std::string message, bool is_error)
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        status_message  = std::move(message);
        status_is_error = is_error;
    };

    auto start_search = [&]()
    {
        if (search_running)
        {
            return;
        }

        if (search_thread.joinable())
        {
            search_thread.join();
        }

        RosterGrid search_grid;
        int search_start_day = 0;
        int search_stop_day  = 6;

        {
            std::lock_guard<std::mutex> lock(state_mutex);
            search_grid       = grid;
            search_start_day  = start_day_index;
            search_stop_day   = stop_day_index;
            found_count       = 0;
            status_message    = "Searching...";
            status_is_error   = false;
            search_running    = true;
        }

        screen.PostEvent(ftxui::Event::Custom);

        search_thread = std::thread(
            [&, search_grid, search_start_day, search_stop_day]()
            {
                try
                {
                    auto planning = build_week_planning(search_grid, search_start_day, search_stop_day);

                    std::ofstream output(output_file_path);
                    if (!output)
                    {
                        throw std::runtime_error("Unable to open " + output_file_path + " for writing");
                    }

                    PlanningMaster planning_master;
                    std::size_t local_found_count = 0;
                    planning_master.start_search(
                        days[static_cast<std::size_t>(search_start_day)], planning, days[static_cast<std::size_t>(search_stop_day)],
                        [&](WeekPlanning result)
                        {
                            output << "Result " << (local_found_count + 1) << '\n';
                            result.print(output);
                            output << '\n';
                            ++local_found_count;
                        });

                    {
                        std::lock_guard<std::mutex> lock(state_mutex);
                        found_count     = local_found_count;
                        status_message  = "Found results: " + std::to_string(found_count) + ". Results written to " + output_file_path + ".";
                        status_is_error = false;
                    }
                }
                catch (const std::exception& ex)
                {
                    std::lock_guard<std::mutex> lock(state_mutex);
                    status_message  = std::string("Search failed: ") + ex.what();
                    status_is_error = true;
                }

                search_running = false;
                screen.PostEvent(ftxui::Event::Custom);
            });
    };

    ftxui::Components grid_rows;
    grid_rows.reserve(nr_of_persons);

    for (std::size_t person = 0; person < nr_of_persons; ++person)
    {
        ftxui::Components row_buttons;
        row_buttons.reserve(nr_of_days);

        for (std::size_t day = 0; day < nr_of_days; ++day)
        {
            auto button = ftxui::Button(
                              &cell_labels[person][day],
                              [&, person, day]()
                              {
                                  if (search_running)
                                  {
                                      return;
                                  }

                                  const auto label_count = is_weekend(day) ? weekend_shift_labels.size() : weekday_shift_labels.size();
                                  grid[person][day]      = (grid[person][day] + 1) % static_cast<int>(label_count);
                                  cell_labels[person][day] = cell_label(grid, person, day);
                                  set_status("", false);
                              },
                              ftxui::ButtonOption::Ascii())
                          | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, cell_width);

            row_buttons.push_back(button);
        }

        grid_rows.push_back(ftxui::Container::Horizontal(row_buttons));
    }

    auto grid_container = ftxui::Container::Vertical(grid_rows);
    auto start_day      = ftxui::Toggle(&day_labels, &start_day_index);
    auto stop_day       = ftxui::Toggle(&day_labels, &stop_day_index);
    auto search_button  = ftxui::Button("Search", start_search, ftxui::ButtonOption::Ascii());
    auto quit_button    = ftxui::Button(
        "Quit",
        [&]()
        {
            if (!search_running)
            {
                screen.ExitLoopClosure()();
            }
        },
        ftxui::ButtonOption::Ascii());

    auto controls = ftxui::Container::Vertical({
        start_day,
        stop_day,
        ftxui::Container::Horizontal({
            search_button,
            quit_button,
        }),
    });

    auto root_container = ftxui::Container::Vertical({
        controls,
        grid_container,
    });

    auto root = ftxui::Renderer(
        root_container,
        [&]()
        {
            std::string local_status;
            bool local_status_is_error = false;
            std::size_t local_found_count = 0;
            const bool local_search_running = search_running;

            {
                std::lock_guard<std::mutex> lock(state_mutex);
                local_status          = status_message;
                local_status_is_error = status_is_error;
                local_found_count     = found_count;
            }

            ftxui::Elements header_cells;
            header_cells.push_back(ftxui::text("person") | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8) | ftxui::bold);
            for (const auto& label : day_labels)
            {
                header_cells.push_back(ftxui::text(label) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, cell_width) | ftxui::bold);
            }

            ftxui::Elements rendered_rows;
            rendered_rows.push_back(ftxui::hbox(std::move(header_cells)));

            for (std::size_t person = 0; person < nr_of_persons; ++person)
            {
                rendered_rows.push_back(ftxui::hbox({
                    ftxui::text(std::to_string(person)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8),
                    grid_container->ChildAt(static_cast<int>(person))->Render(),
                }));
            }

            return ftxui::window(
                ftxui::text("Roster Slayer"),
                ftxui::vbox({
                    ftxui::hbox({
                        ftxui::text("Start "),
                        start_day->Render(),
                    }),
                    ftxui::hbox({
                        ftxui::text("Stop  "),
                        stop_day->Render(),
                    }),
                    ftxui::hbox({
                        search_button->Render(),
                        ftxui::text(" "),
                        quit_button->Render(),
                    }),
                    ftxui::text("Results file: " + output_file_path) | ftxui::bold | ftxui::color(ftxui::Color::CyanLight),
                    ftxui::separator(),
                    ftxui::vbox(std::move(rendered_rows)) | ftxui::xframe | ftxui::yframe,
                    ftxui::separator(),
                    render_status(local_status, local_status_is_error, local_search_running, local_found_count),
                    ftxui::text("X = optional/open. Other values are locked before search.") | ftxui::color(ftxui::Color::GrayLight),
                    ftxui::text("Only locked cells inside the selected start/stop range are used.") | ftxui::color(ftxui::Color::GrayLight),
                }))
                | ftxui::border;
        });

    root |= ftxui::CatchEvent(
        [&](ftxui::Event event)
        {
            if (event == ftxui::Event::Custom)
            {
                return false;
            }

            return search_running.load();
        });

    screen.Loop(root);

    if (search_thread.joinable())
    {
        search_thread.join();
    }

    return 0;
}
