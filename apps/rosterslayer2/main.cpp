#include "days_of_the_week.hpp"
#include "planning_master.hpp"

#include <iostream>

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    PlanningMaster planning_master;
    WeekPlanning planning;
    planning_master.start_search(DaysOfTheWeek::monday, planning, DaysOfTheWeek::sunday, [](WeekPlanning result) { result.print(std::cout); });

    return 0;
}
