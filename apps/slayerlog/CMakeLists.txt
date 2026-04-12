project(slayerlog)

add_executable(
  slayerlog
  main.cpp
  command_palette_controller.cpp
  commands/command_palette_controller.hpp
  commands/command_palette_model.hpp
  commands/command_palette_view.cpp
  commands/command_palette_view.hpp
  command_history.cpp
  command_history.hpp
  commands/command_manager.cpp
  commands/command_manager.hpp
  command_line_parser.cpp
  command_line_parser.hpp
  debug_log.hpp
  watchers/file_watcher.cpp
  watchers/file_watcher.hpp
  log_source.cpp
  log_source.hpp
  log_watcher.hpp
  log_controller.cpp
  log_controller.hpp
  log_batch.cpp
  log_batch.hpp
  log_view.cpp
  log_view.hpp
  master_controller.cpp
  master_controller.hpp
  master_view.cpp
  master_view.hpp
  log_timestamp.cpp
  log_timestamp.hpp
  process_pipe.cpp
  process_pipe.hpp
  settings_ini.cpp
  settings_ini.hpp
  settings_store.cpp
  settings_store.hpp
  watchers/ssh_tail_watcher.cpp
  watchers/ssh_tail_watcher.hpp
  stream_line_buffer.cpp
  stream_line_buffer.hpp
  log_model.cpp
  log_model.hpp
  log4cplus.ini
  view_theme.hpp)

target_compile_definitions(slayerlog PRIVATE BOOST_ALL_NO_LIB BOOST_USE_WINAPI_VERSION=0x0602)

target_include_directories(slayerlog PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/commands)

target_link_libraries(
  slayerlog
  PRIVATE component
          ftxui_components
          Boost::program_options
          log4cplus::log4cplus)

add_custom_command(
  TARGET slayerlog
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/log4cplus.ini"
          "$<TARGET_FILE_DIR:slayerlog>/log4cplus.ini"
  COMMENT "Copy log4cplus.ini next to slayerlog executable")
