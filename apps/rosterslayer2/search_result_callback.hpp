#pragma once

#include "week_planning.hpp"

#include <functional>

using SearchResultCallback = std::function<void(WeekPlanning result)>;
