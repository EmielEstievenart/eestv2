add_executable(
  fxtui_text_view_example
  main.cpp
  text_view_model.cpp
  text_view_model.hpp
  text_view_controller.cpp
  text_view_controller.hpp
  text_view_view.cpp
  text_view_view.hpp)

target_link_libraries(fxtui_text_view_example PRIVATE component)
