cmake_minimum_required(VERSION 3.21)

# Specify which Boost libraries to include (limits build to essentials; add more
# if needed)
set(BOOST_INCLUDE_LIBRARIES asio system program_options) # 'system' is often
                                                         # needed for ASIO
# error handling
set(BOOST_ENABLE_CMAKE ON)

# Add Boost as a subdirectory using the env var
add_subdirectory($ENV{BOOST_ROOT} my_boost)

project(academic)

# Set output directories for all targets globally
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Set C++17 standard globally
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# On Windows ensure a minimum _WIN32_WINNT is defined so system headers (and
# Boost.Asio) target the correct Win32 APIs and avoid warnings.
if(WIN32)
  add_compile_definitions(_WIN32_WINNT=0x0601)
endif()

enable_testing()

add_subdirectory(code)
add_subdirectory(examples)
add_subdirectory(tests/unit_tests)
