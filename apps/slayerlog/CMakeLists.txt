project(slayerlog)

add_executable(
  slayerlog
  main.cpp
  command_palette_controller.cpp
  command_palette_controller.hpp
  command_palette_model.hpp
  command_manager.cpp
  command_manager.hpp
  command_line_parser.cpp
  command_line_parser.hpp
  debug_log.hpp
  file_watcher.cpp
  file_watcher.hpp
  input_controller.cpp
  input_controller.hpp
  log_batch.cpp
  log_batch.hpp
  log_view.cpp
  log_view.hpp
  log_timestamp.cpp
  log_timestamp.hpp
  log_view_model.cpp
  log_view_model.hpp)

target_compile_definitions(slayerlog PRIVATE BOOST_ALL_NO_LIB BOOST_USE_WINAPI_VERSION=0x0602)

target_link_libraries(
  slayerlog
  PRIVATE component
          Boost::program_options
          Boost::log
          Boost::log_setup
          Boost::filesystem
          Boost::thread)
