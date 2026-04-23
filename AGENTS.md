# Repository Guidelines

Before any edits, confirm the working directory with pwd!

## Project Structure & Module Organization
`code/eestv/` contains the reusable library code, grouped by domain such as `serial/`, `net/`, `data_bridge/`, `logging/`, and `threading/`. `apps/` contains executable applications, currently including `apps/slayerlog/`. `examples/` holds small focused sample programs, while `tests/unit_tests/` mirrors the production layout with GoogleTest coverage. Third-party dependencies live under `libs/`, and generated build output goes to `out/build/<preset>/`.

## Build, Test, and Development Commands
Set `BOOST_ROOT` to a Boost source checkout before configuring; this project uses `add_subdirectory()` for Boost.

Run CMake build commands for this repository outside the sandbox. Request escalation before configuring or building so the build can use the real toolchain and filesystem state.

```bash
cmake --preset windows-clang-debug
cmake --build --preset windows-clang-debug
ctest --preset windows-clang-debug
```

Use the matching Linux presets on Linux, for example `linux-gcc-debug` or `linux-gcc-release`. For a full local cycle, run `cmake --workflow --preset windows-msvc-release`. To launch the main app:

```bash
out/build/windows-msvc-release/bin/slayerlog --file path/to/log.txt
```

## Coding Style & Naming Conventions
The repo is C++17-based. Formatting is defined in `.clang-format`: 4-space indentation, no tabs, Allman braces, and a 140-column limit. Keep include order stable and do not enable automatic include sorting that conflicts with the existing config. Use `snake_case` for files and most functions, `PascalCase` for types, and keep test names descriptive, for example `LogTimestampTest.ParsesIsoTimestamps`.

## Testing Guidelines
Unit tests use GoogleTest and live under `tests/unit_tests/<area>/`. Name new test files by feature, such as `log_timestamp_tests.cpp`, and keep individual test names behavior-focused. Run the full suite with `ctest --preset <preset>` or execute the built `unit_tests` binary with `--gtest_filter=SuiteName.*` for targeted work.

## Commit & Pull Request Guidelines
Recent commits use short imperative subjects like `Add data_bridge unit tests` and targeted prefixes like `Slayerlog: add support for multiple files and timestamps`. Follow that pattern: one line, present tense, scoped when useful. Pull requests should summarize the behavioral change, list validation performed, link any related issue, and include screenshots or terminal output when UI or CLI behavior changes.

## Configuration Notes
Initialize submodules after cloning with `git submodule update --init --recursive`. Do not commit files from `out/` or machine-specific Boost paths.
