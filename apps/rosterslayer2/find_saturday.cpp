#include "find_saturday.hpp"

#include "one_day_planning.hpp"
#include "weekend_shifts.hpp"

void find_possible_saturdays()
{
    OneDayPlanning<WeekendShiftCode> saturday_planning(get_weekend_required_shifts());
}
