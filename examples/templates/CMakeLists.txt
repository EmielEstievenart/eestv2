cmake_minimum_required(VERSION 3.20)

# Create the templates executable
add_executable(templates_example main.cpp)

# Set C++ standard
target_compile_features(templates_example PRIVATE cxx_std_17)

# Add compiler-specific flags for better warnings and debugging
if(MSVC)
  target_compile_options(templates_example PRIVATE /W4)
else()
  target_compile_options(templates_example PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Ensure the header files are in the include path
target_include_directories(templates_example
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
