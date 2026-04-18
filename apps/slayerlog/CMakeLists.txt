project(slayerlog)

add_executable(
  slayerlog
  main.cpp
  tracked_sources/all_tracked_sources.cpp
  tracked_sources/all_tracked_sources.hpp
  tracked_sources/tracked_source.cpp
  tracked_sources/tracked_source.hpp
  command_line_parser.cpp
  command_line_parser.hpp
  debug_log.hpp
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
  log_types.hpp
  settings_ini.cpp
  settings_ini.hpp
  settings_store.cpp
  settings_store.hpp
  tracked_sources/all_processed_sources.cpp
  tracked_sources/all_processed_sources.hpp
  search_pattern.cpp
  search_pattern.hpp
  log4cplus.ini
  view_theme.hpp
  commands/command_palette_controller.cpp
  commands/command_palette_controller.hpp
  commands/command_palette_model.hpp
  commands/command_palette_view.cpp
  commands/command_palette_view.hpp
  commands/command_registrar.cpp
  commands/command_registrar.hpp
  commands/command_history.cpp
  commands/command_history.hpp
  commands/command_manager.cpp
  commands/command_manager.hpp
  watchers/process_pipe.cpp
  watchers/process_pipe.hpp
  watchers/ssh_tail_watcher.cpp
  watchers/ssh_tail_watcher.hpp
  watchers/stream_line_buffer.cpp
  watchers/stream_line_buffer.hpp
  watchers/log_watcher_base.hpp
  watchers/file_watcher.cpp
  watchers/file_watcher.hpp
  watchers/zstd_file_watcher.cpp
  watchers/zstd_file_watcher.hpp)

target_compile_definitions(slayerlog PRIVATE BOOST_ALL_NO_LIB
                                             BOOST_USE_WINAPI_VERSION=0x0602)

target_include_directories(
  slayerlog PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/commands
                    ${CMAKE_CURRENT_SOURCE_DIR}/tracked_sources
                    ${CMAKE_SOURCE_DIR}/code)

target_link_libraries(
  slayerlog PRIVATE component ftxui_components Boost::program_options
                    log4cplus::log4cplus libzstd eestv_lib)

add_custom_command(
  TARGET slayerlog
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/log4cplus.ini"
    "$<TARGET_FILE_DIR:slayerlog>/log4cplus.ini"
  COMMENT "Copy log4cplus.ini next to slayerlog executable")
