cmake_minimum_required(VERSION 3.15)
project(variadic_templates_example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(variadic_templates main.cpp)
target_compile_features(variadic_templates PRIVATE cxx_std_17)
