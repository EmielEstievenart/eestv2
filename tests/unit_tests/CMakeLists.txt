# Add executable for unit tests
add_executable(
  unit_tests
  ftxui_components/text_view_controller_tests.cpp
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
  slayerlog/log_source_tests.cpp
  slayerlog/stream_line_buffer_tests.cpp
  slayerlog/command_palette_controller_tests.cpp
  slayerlog/command_line_parser_tests.cpp
  slayerlog/command_history_tests.cpp
  slayerlog/command_manager_tests.cpp
  slayerlog/log_batch_tests.cpp
  slayerlog/log_timestamp_tests.cpp
  slayerlog/log_model_tests.cpp
  slayerlog/log_controller_tests.cpp
  slayerlog/master_controller_tests.cpp
  slayerlog/settings_ini_tests.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/command_palette_controller.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/command_palette_controller.hpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/command_palette_model.hpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/command_palette_view.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/command_palette_view.hpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/command_line_parser.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/command_line_parser.hpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/command_history.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/command_history.hpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/command_manager.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/command_manager.hpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/debug_log.hpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/watchers/file_watcher.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/log_source.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/log_controller.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/log_batch.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/log_view.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/log_timestamp.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/master_controller.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/master_controller.hpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/master_view.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/master_view.hpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/process_pipe.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/settings_ini.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/settings_ini.hpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/settings_store.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/settings_store.hpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/watchers/ssh_tail_watcher.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/stream_line_buffer.cpp
  ${CMAKE_SOURCE_DIR}/apps/slayerlog/log_model.cpp)

# Link libraries (assuming eestv_lib provides sources and Boost)
target_link_libraries(
  unit_tests
  PRIVATE eestv_lib
          ftxui_components
          component
          GTest::gtest_main
          Boost::log
          Boost::log_setup
          Boost::filesystem
          Boost::thread
          Boost::program_options
          log4cplus::log4cplus)

target_compile_definitions(unit_tests PRIVATE BOOST_ALL_NO_LIB BOOST_USE_WINAPI_VERSION=0x0602)
target_include_directories(unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/apps/slayerlog)

# Conditionally discover tests (disabled for TSAN where it doesn't work well)
if(ENABLE_CTEST)
  include(GoogleTest)
  gtest_discover_tests(unit_tests DISCOVERY_MODE PRE_TEST)
endif()
