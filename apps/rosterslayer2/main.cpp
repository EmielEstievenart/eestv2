#include "days_of_the_week.hpp"
#include "find_saturday.hpp"

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    DaysOfTheWeek search_until = DaysOfTheWeek::friday;
    find_possible_saturdays(search_until);

    return 0;
}
