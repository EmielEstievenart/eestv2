#include "week_planning.hpp"

#pragma once

class NursingValidator
{
public:
    NursingValidator(int nr_of_nurses, int nr_of_nursing_assistants);

    bool validate(const WeekPlanning& planning) const;

private:
    int _nr_of_nurses;
    int _nr_of_nursing_assistants;
};