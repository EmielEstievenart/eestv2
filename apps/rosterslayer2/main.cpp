#include "days_of_the_week.hpp"
#include "planning_master.hpp"
#include "weekend_shifts.hpp"

#include <iostream>

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    PlanningMaster planning_master;
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

    planning_master.start_search(DaysOfTheWeek::monday, planning, DaysOfTheWeek::thursday, [](WeekPlanning result) { result.print(std::cout); });

    return 0;
}
