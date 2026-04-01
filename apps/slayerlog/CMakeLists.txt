project(slayerlog)

add_executable(
  slayerlog
  main.cpp
  command_line_parser.cpp
  command_line_parser.hpp
  file_watcher.cpp
  file_watcher.hpp
  input_controller.cpp
  input_controller.hpp
  log_view.cpp
  log_view.hpp
  log_timestamp.cpp
  log_timestamp.hpp
  log_view_model.cpp
  log_view_model.hpp)

target_link_libraries(slayerlog PRIVATE component Boost::program_options)
