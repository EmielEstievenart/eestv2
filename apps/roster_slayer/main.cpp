#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <exception>
#include <future>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <chrono>
#include <vector>

#include "day_pruning.hpp"
#include "daily_set.hpp"
#include "roster_validation.hpp"
#include "weekday_shifts.hpp"
#include "weekend_shifts.hpp"

namespace
{
std::atomic_uint64_t printed_week_count {0};
std::atomic_uint64_t skipped_frontier_branch_count {0};
std::atomic_uint64_t skipped_recursive_branch_count {0};
std::atomic_uint64_t candidate_day_attempt_count {0};
std::atomic_uint64_t candidate_day_accept_count {0};
std::atomic_uint64_t candidate_day_reject_consecutive_count {0};
std::atomic_uint64_t candidate_day_reject_partial_week_count {0};
std::mutex output_mutex;
std::mutex progress_mutex;
auto last_progress_log_time = std::chrono::steady_clock::time_point {};
constexpr const char* valid_weeks_output_path = "apps/roster_slayer/valid_weeks.txt";

void debug_log(const std::string& message)
{
    std::lock_guard<std::mutex> lock(output_mutex);
    std::cerr << message << std::endl;
}

void log_progress_summary_if_due(const char* source)
{
    const auto now = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(progress_mutex);
    if ((last_progress_log_time != std::chrono::steady_clock::time_point {}) &&
        (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_progress_log_time).count() < 1000))
    {
        return;
    }

    last_progress_log_time = now;
    debug_log(std::string("Progress[") + source + "] attempts=" + std::to_string(candidate_day_attempt_count.load()) +
              ", accepted=" + std::to_string(candidate_day_accept_count.load()) +
              ", reject_consecutive=" + std::to_string(candidate_day_reject_consecutive_count.load()) +
              ", reject_partial_week=" + std::to_string(candidate_day_reject_partial_week_count.load()) +
              ", skipped_frontier=" + std::to_string(skipped_frontier_branch_count.load()) +
              ", skipped_recursive=" + std::to_string(skipped_recursive_branch_count.load()));
}

[[nodiscard]] bool find_sunday(std::vector<roster_slayer::DailySet>& valid_week);

[[nodiscard]] bool find_monday(std::vector<roster_slayer::DailySet>& valid_week);

[[nodiscard]] bool find_tuesday(std::vector<roster_slayer::DailySet>& valid_week);

[[nodiscard]] bool find_wednesday(std::vector<roster_slayer::DailySet>& valid_week);

[[nodiscard]] bool find_thursday(std::vector<roster_slayer::DailySet>& valid_week);

[[nodiscard]] std::vector<roster_slayer::DailySet> find_saturday();

using DayValidator = bool (*)(const roster_slayer::DailySet& current_day, const roster_slayer::DailySet& candidate_day);

using NextDayFinder = bool (*)(std::vector<roster_slayer::DailySet>& valid_week);

struct PrunedWeekdayBranch
{
    roster_slayer::DailySet template_set;
    std::size_t next_person_index;
};

constexpr std::array<const char*, 7> day_names {"Sat", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri"};

[[nodiscard]] const char* current_day_name(std::size_t valid_week_size)
{
    return valid_week_size < day_names.size() ? day_names[valid_week_size] : "Day";
}

[[nodiscard]] bool try_to_add_day(std::vector<roster_slayer::DailySet>& valid_week,
                                  const roster_slayer::DailySet& day_template,
                                  DayValidator validator,
                                  NextDayFinder next_day_finder = nullptr);

void build_pruned_weekday_frontier(const roster_slayer::DailySet& previous_day,
                                   roster_slayer::DailySet current_template,
                                   std::size_t person_index,
                                   std::size_t target_branch_count,
                                   std::vector<PrunedWeekdayBranch>& frontier)
{
    if (frontier.size() + 1 >= target_branch_count)
    {
        frontier.push_back({current_template, person_index});
        return;
    }

    for (std::size_t current_person_index = person_index; current_person_index < current_template.size(); ++current_person_index)
    {
        const roster_slayer::Shift& previous_shift = previous_day.get(current_person_index);
        if (current_template.is_set(current_person_index))
        {
            if (!roster_slayer::day_pruning::is_allowed_next_weekday_shift(previous_shift, current_template.get(current_person_index)))
            {
                return;
            }

            continue;
        }

        for (const roster_slayer::Shift& allowed_shift : roster_slayer::day_pruning::get_allowed_next_weekday_shifts(previous_shift))
        {
            auto next_template = current_template;
            try
            {
                next_template.set(current_person_index, allowed_shift);
            }
            catch (const std::exception& exception)
            {
                (void)exception;
                skipped_frontier_branch_count.fetch_add(1);
                log_progress_summary_if_due("frontier");
                continue;
            }

            build_pruned_weekday_frontier(previous_day, next_template, current_person_index + 1, target_branch_count, frontier);
        }

        return;
    }

    frontier.push_back({current_template, current_template.size()});
}

[[nodiscard]] bool try_to_add_pruned_weekday_recursive(const roster_slayer::DailySet& previous_day,
                                                       std::vector<roster_slayer::DailySet>& valid_week,
                                                       roster_slayer::DailySet current_template,
                                                       std::size_t person_index,
                                                       DayValidator validator,
                                                       NextDayFinder next_day_finder)
{
    for (std::size_t current_person_index = person_index; current_person_index < current_template.size(); ++current_person_index)
    {
        const roster_slayer::Shift& previous_shift = previous_day.get(current_person_index);
        if (current_template.is_set(current_person_index))
        {
            if (!roster_slayer::day_pruning::is_allowed_next_weekday_shift(previous_shift, current_template.get(current_person_index)))
            {
                return false;
            }

            continue;
        }

        bool found_valid_week = false;
        for (const roster_slayer::Shift& allowed_shift : roster_slayer::day_pruning::get_allowed_next_weekday_shifts(previous_shift))
        {
            auto next_template = current_template;
            try
            {
                next_template.set(current_person_index, allowed_shift);
            }
            catch (const std::exception& exception)
            {
                (void)exception;
                skipped_recursive_branch_count.fetch_add(1);
                log_progress_summary_if_due("recursive");
                continue;
            }

            if (try_to_add_pruned_weekday_recursive(previous_day, valid_week, next_template, current_person_index + 1, validator, next_day_finder))
            {
                found_valid_week = true;
            }
        }

        return found_valid_week;
    }

    return try_to_add_day(valid_week, current_template, validator, next_day_finder);
}

[[nodiscard]] bool try_to_add_pruned_weekday(std::vector<roster_slayer::DailySet>& valid_week,
                                             const roster_slayer::DailySet& day_template,
                                             DayValidator validator,
                                             NextDayFinder next_day_finder = nullptr)
{
    debug_log(std::string("Entering ") + current_day_name(valid_week.size()) + " pruning with previous day index=" + std::to_string(valid_week.size() - 1) +
              ", pre-fixed entries=" + std::to_string(day_template.nr_of_assigned_entries()));

    const roster_slayer::DailySet& previous_day = valid_week.back();
    std::size_t first_unset_person_index        = day_template.size();
    for (std::size_t person_index = 0; person_index < day_template.size(); ++person_index)
    {
        if (!day_template.is_set(person_index))
        {
            first_unset_person_index = person_index;
            break;
        }

        if (!roster_slayer::day_pruning::is_allowed_next_weekday_shift(previous_day.get(person_index), day_template.get(person_index)))
        {
            debug_log(std::string("Pruned ") + current_day_name(valid_week.size()) + " immediately due to incompatible fixed assignment at person " + std::to_string(person_index));
            return false;
        }
    }

    if (first_unset_person_index == day_template.size())
    {
        debug_log(std::string("No pruning branches for ") + current_day_name(valid_week.size()) + ", using fixed template directly");
        return try_to_add_day(valid_week, day_template, validator, next_day_finder);
    }

    const std::size_t hardware_workers    = std::max<std::size_t>(1, std::thread::hardware_concurrency());
    const std::size_t target_branch_count = std::max<std::size_t>(1, hardware_workers * 2);
    std::vector<PrunedWeekdayBranch> frontier;
    frontier.reserve(target_branch_count);
    build_pruned_weekday_frontier(previous_day, day_template, first_unset_person_index, target_branch_count, frontier);

    debug_log("Launching " + std::to_string(frontier.size()) + " worker branch(es) for " + current_day_name(valid_week.size()) +
              " with hardware concurrency=" + std::to_string(hardware_workers));

    if (frontier.size() <= 1)
    {
        bool found_valid_week = false;
        for (const PrunedWeekdayBranch& branch : frontier)
        {
            if (try_to_add_pruned_weekday_recursive(previous_day, valid_week, branch.template_set, branch.next_person_index, validator, next_day_finder))
            {
                found_valid_week = true;
            }
        }

        return found_valid_week;
    }

    std::vector<std::future<bool>> futures;
    futures.reserve(frontier.size());

    for (std::size_t worker_index = 0; worker_index < frontier.size(); ++worker_index)
    {
        const PrunedWeekdayBranch branch = frontier[worker_index];

        futures.push_back(std::async(std::launch::async,
                                     [worker_index, &previous_day, &valid_week, branch, validator, next_day_finder]() mutable
                                     {
                                         try
                                         {
                                             debug_log("Worker " + std::to_string(worker_index) + " exploring branch from person " + std::to_string(branch.next_person_index));

                                             bool found_valid_week = false;
                                             auto valid_week_copy  = valid_week;
                                             if (try_to_add_pruned_weekday_recursive(previous_day,
                                                                                    valid_week_copy,
                                                                                    branch.template_set,
                                                                                    branch.next_person_index,
                                                                                    validator,
                                                                                    next_day_finder))
                                             {
                                                 found_valid_week = true;
                                             }

                                             debug_log("Worker " + std::to_string(worker_index) + " finished, found_valid_week=" + (found_valid_week ? std::string("true") : std::string("false")));

                                             return found_valid_week;
                                         }
                                         catch (const std::exception& exception)
                                         {
                                             debug_log("Worker " + std::to_string(worker_index) + " failed: " + exception.what());
                                             return false;
                                         }
                                     }));
    }

    bool found_valid_week = false;
    for (auto& future : futures)
    {
        try
        {
            if (future.get())
            {
                found_valid_week = true;
            }
        }
        catch (const std::exception& exception)
        {
            debug_log(std::string("Async pruning worker raised exception: ") + exception.what());
        }
    }

    return found_valid_week;
}

[[nodiscard]] roster_slayer::DailySet make_weekday_template()
{
    return roster_slayer::DailySet {roster_slayer::weekday_shifts::get_weekly_required_shifts()};
}

[[nodiscard]] roster_slayer::DailySet make_weekend_template()
{
    return roster_slayer::DailySet {roster_slayer::weekend_shifts::get_weekly_required_shifts()};
}

void force_off_after_four_consecutive_work_days(const std::vector<roster_slayer::DailySet>& valid_week, roster_slayer::DailySet& day_template, const roster_slayer::Shift& off_shift)
{
    if (valid_week.size() < 4)
    {
        return;
    }

    const std::size_t current_day_index = valid_week.size();
    if (current_day_index < 4)
    {
        return;
    }

    for (std::size_t person_index = 0; person_index < day_template.size(); ++person_index)
    {
        bool worked_previous_four_days = true;
        for (std::size_t day_offset = 4; day_offset > 0; --day_offset)
        {
            if (valid_week[current_day_index - day_offset].get(person_index).is_off())
            {
                worked_previous_four_days = false;
                break;
            }
        }

        if (worked_previous_four_days)
        {
            day_template.set(person_index, off_shift);
        }
    }
}

void print_valid_week(std::ostream& out, const std::vector<roster_slayer::DailySet>& valid_week)
{
    if (valid_week.empty())
    {
        return;
    }

    constexpr int column_width = 8;

    out << std::left << std::setw(column_width) << "Row";
    for (std::size_t day_index = 0; day_index < valid_week.size(); ++day_index)
    {
        const char* day_name = day_index < day_names.size() ? day_names[day_index] : "Day";
        out << std::setw(column_width) << day_name;
    }
    out << "\n";

    const std::size_t row_count = valid_week.front().size();
    for (std::size_t row_index = 0; row_index < row_count; ++row_index)
    {
        out << std::setw(column_width) << row_index;

        for (const auto& day : valid_week)
        {
            out << std::setw(column_width) << day.get(row_index).get_code();
        }

        out << "\n";
    }
}

void print_valid_week(const std::vector<roster_slayer::DailySet>& valid_week)
{
    print_valid_week(std::cout, valid_week);
}

void print_thursday_rejection(const std::vector<roster_slayer::DailySet>& valid_week, const roster_slayer::DailySet& candidate_day, const std::string& reason)
{
    if (valid_week.size() != 5)
    {
        return;
    }

    std::cout << "Thursday candidate rejected: " << reason << "\n";

    auto rejected_week = valid_week;
    rejected_week.push_back(candidate_day);
    print_valid_week(rejected_week);
    std::cout << "\n";
}

[[nodiscard]] bool try_to_add_day(std::vector<roster_slayer::DailySet>& valid_week, const roster_slayer::DailySet& day_template, DayValidator validator, NextDayFinder next_day_finder)
{
    bool found_valid_week                 = false;
    const std::uint64_t combination_count = day_template.get_nr_of_combinations();
    debug_log(std::string("Enumerating ") + current_day_name(valid_week.size()) + " with " + std::to_string(combination_count) + " combination(s)");

    for (std::uint64_t combination_index = 0; combination_index < combination_count; ++combination_index)
    {
        candidate_day_attempt_count.fetch_add(1);
        if (combination_index > 0 && (combination_index % 1000) == 0)
        {
            debug_log(std::string("Progress ") + current_day_name(valid_week.size()) + ": " + std::to_string(combination_index) + "/" + std::to_string(combination_count));
        }

        const auto candidate_day = day_template.get_set(combination_index);
        if (!validator(valid_week.back(), candidate_day))
        {
            candidate_day_reject_consecutive_count.fetch_add(1);
            log_progress_summary_if_due(current_day_name(valid_week.size()));
            // print_thursday_rejection(valid_week, candidate_day, roster_slayer::explain_2_weekday_shift_rejection(valid_week.back(), candidate_day));
            continue;
        }

        if (!roster_slayer::verify_candidate_day_with_partial_week(valid_week, candidate_day))
        {
            candidate_day_reject_partial_week_count.fetch_add(1);
            log_progress_summary_if_due(current_day_name(valid_week.size()));
            // print_thursday_rejection(valid_week, candidate_day, roster_slayer::explain_candidate_day_rejection(valid_week, candidate_day));
            continue;
        }
        candidate_day_accept_count.fetch_add(1);
        log_progress_summary_if_due(current_day_name(valid_week.size()));

        valid_week.push_back(candidate_day);
        if (next_day_finder == nullptr)
        {
            const std::uint64_t week_number = printed_week_count.fetch_add(1) + 1;
            {
                std::lock_guard<std::mutex> lock(output_mutex);
                std::ofstream out_file {valid_weeks_output_path, std::ios::app};
                if (!out_file)
                {
                    throw std::runtime_error(std::string("Failed to open valid week output file: ") + valid_weeks_output_path);
                }

                std::cout << "Valid week " << week_number << ":\n";
                print_valid_week(valid_week);
                std::cout << "\n";

                out_file << "Valid week " << week_number << ":\n";
                print_valid_week(out_file, valid_week);
                out_file << "\n";
            }
            found_valid_week = true;
        }
        else if (next_day_finder(valid_week))
        {
            found_valid_week = true;
        }

        valid_week.pop_back();
    }

    return found_valid_week;
}

[[nodiscard]] std::vector<roster_slayer::DailySet> find_saturday()
{
    auto saturday_template = make_weekend_template();

    saturday_template.set(0, roster_slayer::weekend_shifts::get_off_shift());
    saturday_template.set(2, roster_slayer::weekend_shifts::get_off_shift());
    saturday_template.set(4, roster_slayer::weekend_shifts::get_off_shift());
    saturday_template.set(6, roster_slayer::weekend_shifts::get_off_shift());
    saturday_template.set(8, roster_slayer::weekend_shifts::get_off_shift());

    std::vector<roster_slayer::DailySet> first_valid_week;
    const std::uint64_t combination_count = saturday_template.get_nr_of_combinations();
    debug_log("Enumerating Sat with " + std::to_string(combination_count) + " combination(s)");
    for (std::uint64_t combination_index = 0; combination_index < combination_count; ++combination_index)
    {
        if (combination_index > 0 && (combination_index % 1000) == 0)
        {
            debug_log("Progress Sat: " + std::to_string(combination_index) + "/" + std::to_string(combination_count));
        }

        std::vector<roster_slayer::DailySet> valid_week;
        valid_week.push_back(saturday_template.get_set(combination_index));

        if (find_sunday(valid_week) && first_valid_week.empty())
        {
            first_valid_week = valid_week;
        }
    }

    return first_valid_week;
}

[[nodiscard]] bool find_sunday(std::vector<roster_slayer::DailySet>& valid_week)
{
    auto sunday_template = make_weekend_template();

    sunday_template.set(0, roster_slayer::weekend_shifts::get_off_shift());
    sunday_template.set(2, roster_slayer::weekend_shifts::get_off_shift());
    sunday_template.set(4, roster_slayer::weekend_shifts::get_off_shift());
    sunday_template.set(6, roster_slayer::weekend_shifts::get_off_shift());
    sunday_template.set(8, roster_slayer::weekend_shifts::get_off_shift());

    debug_log("Entering Sun search for current Sat candidate");

    return try_to_add_day(valid_week, sunday_template, roster_slayer::verify_2_weekend_shifts_allowed_consecutively, find_monday);
}

[[nodiscard]] bool find_monday(std::vector<roster_slayer::DailySet>& valid_week)
{
    return try_to_add_pruned_weekday(valid_week, make_weekday_template(), roster_slayer::verify_2_shifts_allowed_consecutively, find_tuesday);
}

[[nodiscard]] bool find_tuesday(std::vector<roster_slayer::DailySet>& valid_week)
{
    return try_to_add_pruned_weekday(valid_week, make_weekday_template(), roster_slayer::verify_2_weekday_shifts_allowed_consecutively, find_wednesday);
}

[[nodiscard]] bool find_wednesday(std::vector<roster_slayer::DailySet>& valid_week)
{
    auto wednesday_template = make_weekday_template();
    force_off_after_four_consecutive_work_days(valid_week, wednesday_template, roster_slayer::weekday_shifts::get_off_shift());

    return try_to_add_pruned_weekday(valid_week, wednesday_template, roster_slayer::verify_2_weekday_shifts_allowed_consecutively, find_thursday);
}

[[nodiscard]] bool find_thursday(std::vector<roster_slayer::DailySet>& valid_week)
{
    auto thursday_template = make_weekday_template();
    force_off_after_four_consecutive_work_days(valid_week, thursday_template, roster_slayer::weekday_shifts::get_off_shift());

    return try_to_add_pruned_weekday(valid_week, thursday_template, roster_slayer::verify_2_weekday_shifts_allowed_consecutively);
}
} // namespace

int main()
{
    try
    {
        debug_log("Starting roster_slayer search");
        {
            std::ofstream out_file {valid_weeks_output_path, std::ios::trunc};
            if (!out_file)
            {
                throw std::runtime_error(std::string("Failed to initialize valid week output file: ") + valid_weeks_output_path);
            }
        }
        debug_log(std::string("Writing valid weeks to ") + valid_weeks_output_path);
        const auto valid_week = find_saturday();
        std::cout << "Days found in first valid week: " << valid_week.size() << "\n";

        if (!valid_week.empty())
        {
            print_valid_week(valid_week);
        }
    }
    catch (const std::exception& exception)
    {
        debug_log(std::string("Fatal error: ") + exception.what());
        return 1;
    }
    catch (...)
    {
        debug_log("Fatal error: unknown exception");
        return 1;
    }

    return 0;
}
