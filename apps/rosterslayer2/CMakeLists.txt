project(roster_slayer)

add_executable(
  roster_slayer
  days_of_the_week.hpp
  day_off_planning_validator.cpp
  day_off_planning_validator.hpp
  find_friday.cpp
  find_friday.hpp
  find_monday.cpp
  find_monday.hpp
  find_saturday.cpp
  find_saturday.hpp
  find_sunday.cpp
  find_sunday.hpp
  find_thursday.cpp
  find_thursday.hpp
  find_tuesday.cpp
  find_tuesday.hpp
  find_wednesday.cpp
  find_wednesday.hpp
  main.cpp
  one_day_planning.cpp
  one_day_planning.hpp
  shift.hpp
  shifts_enums.hpp
  search_context.hpp
  search_result_callback.hpp
  time.hpp
  weekday_shifts.hpp
  weekend_shifts.hpp
  double_day_planning_validator.cpp
  double_day_planning_validator.hpp
  planning_master.hpp
  planning_master.cpp)

target_include_directories(
  roster_slayer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                        ${CMAKE_SOURCE_DIR}/libs/magic_enum/include)

target_link_libraries(roster_slayer PRIVATE component)
