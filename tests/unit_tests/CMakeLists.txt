# Enable testing support
enable_testing()

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
FetchContent_MakeAvailable(googletest)

# Add executable for unit tests
add_executable(
  unit_tests
  net/discovery/discovery_integration_tests.cpp
#   net/connection/tcp_connection_tests.cpp
  net/connection/tcp_server_life_cycle.cpp
#   net/connection/tcp_server_connection_life_cycle.cpp
  data/linear_buffer_tests.cpp
  data/linear_buffer_tests.cpp
  serial/serialization_tests.cpp
  data_bridge/data_bridge_cli_tests.cpp
  flags/synchronous_flags_tests.cpp)

# Link libraries (assuming eestv_lib provides sources and Boost)
target_link_libraries(unit_tests PRIVATE eestv_lib GTest::gtest_main)

# Discover all Google Test cases automatically
include(GoogleTest)
gtest_discover_tests(unit_tests)
