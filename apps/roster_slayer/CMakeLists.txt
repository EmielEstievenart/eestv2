project(roster_slayer)

option(BUILD_ROSTER_SLAYER_APP "Build the roster_slayer executable" OFF)

if(BUILD_ROSTER_SLAYER_APP)
    add_executable(roster_slayer main.cpp
        daily_set.cpp)
    target_include_directories(roster_slayer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
else()
    message(STATUS "Skipping roster_slayer executable (BUILD_ROSTER_SLAYER_APP=OFF)")
endif()
