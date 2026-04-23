add_executable(
  async_notify_example
  main.cpp
  can_bus_data_provider.cpp
  can_bus_data_provider.hpp
  can_manager.cpp
  can_manager.hpp
  can_signals.hpp
  can_notify.cpp
  can_notify.hpp
  container.cpp
  container.hpp)

target_compile_features(async_notify_example PRIVATE cxx_std_17)
target_link_libraries(async_notify_example PRIVATE magic_enum::magic_enum)
