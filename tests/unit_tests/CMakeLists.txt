# Add executable for unit tests
add_executable(
  unit_tests
  net/discovery/discovery_client_lifecycle_tests.cpp
  net/discovery/discovery_server_lifecycle_tests.cpp
  net/discovery/discovery_integration_tests.cpp
  net/connection/tcp_connection_life_cycle_tests.cpp
  net/connection/tcp_server_life_cycle_tests.cpp
  net/connection/tcp_client_life_cycle_tests.cpp
  net/connection/tcp_connection_data_transfer_tests.cpp
  net/network_utils_tests.cpp
  data/linear_buffer_tests.cpp
  serial/serialization_tests.cpp
  serial/serialization_mux_tests.cpp
  data_bridge/data_bridge_cli_tests.cpp
  flags/flags_tests.cpp
  slayerlog/file_watcher_tests.cpp
  slayerlog/command_palette_controller_tests.cpp
  slayerlog/command_manager_tests.cpp
  slayerlog/log_batch_tests.cpp
  slayerlog/log_timestamp_tests.cpp
  slayerlog/log_view_model_tests.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/command_palette_controller.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/command_palette_controller.hpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/command_palette_model.hpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/command_manager.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/command_manager.hpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/debug_log.hpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/file_watcher.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/log_batch.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/log_timestamp.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/log_view_model.cpp)

# Link libraries (assuming eestv_lib provides sources and Boost)
target_link_libraries(
  unit_tests
  PRIVATE eestv_lib
          component
          GTest::gtest_main
          Boost::log
          Boost::log_setup
          Boost::filesystem
          Boost::thread)

target_compile_definitions(unit_tests PRIVATE BOOST_ALL_NO_LIB BOOST_USE_WINAPI_VERSION=0x0602)
target_include_directories(unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/apps/slayerlog)

# Conditionally discover tests (disabled for TSAN where it doesn't work well)
if(ENABLE_CTEST)
  include(GoogleTest)
  gtest_discover_tests(unit_tests DISCOVERY_MODE PRE_TEST)
endif()
