project(slayerlog)

add_executable(slayerlog main.cpp command_line_parser.cpp command_line_parser.hpp file_watcher.cpp file_watcher.hpp)

target_link_libraries(slayerlog PRIVATE component Boost::program_options)
