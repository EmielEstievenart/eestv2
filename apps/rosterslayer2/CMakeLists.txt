project(roster_slayer)

add_executable(
  roster_slayer
  days_of_the_week.hpp
  find_saturday.cpp
  find_saturday.hpp
  find_sunday.cpp
  find_sunday.hpp
  main.cpp
  one_day_planning.cpp
  one_day_planning.hpp
  shift.hpp
  shifts_enums.hpp
  time.hpp
  weekday_shifts.hpp
  weekend_shifts.hpp
  double_day_planning_validator.cpp
  double_day_planning_validator.hpp)
target_include_directories(roster_slayer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
