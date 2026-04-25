project(vscompilecommands)

add_executable(
  vscompilecommands
  cli.cpp
  cli.hpp
  compile_commands_printer.cpp
  compile_commands_printer.hpp
  compile_document.hpp
  diagnostics.hpp
  json_parser.cpp
  json_parser.hpp
  main.cpp
  msbuild_document_creator.cpp
  msbuild_document_creator.hpp
  utility.cpp
  utility.hpp)

target_compile_definitions(vscompilecommands PRIVATE _CRT_SECURE_NO_WARNINGS)
