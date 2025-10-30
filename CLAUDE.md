# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the **eestv_lib** project (superproject wrapper) - a C++17 networking and serialization library built on Boost.Asio. The core library (`code/eestv/`) is designed as a lightweight submodule for reuse. This repository includes tests and examples, while the submodule itself has minimal dependencies.

Key modules:
- **net**: TCP client/server/connection classes, UDP discovery client/server
- **serial**: Serialization/deserialization framework with type safety and multiplexing
- **data**: `LinearBuffer` for zero-copy sequential data access
- **data_bridge**: Bridge between data sources and sinks
- **threading**: Threading utilities
- **logging**: Logging utilities
- **flags**: Thread-safe flag utilities

## Prerequisites

- C++17 compiler
- CMake >= 3.21
- Ninja (recommended)
- Boost source tree at `$BOOST_ROOT` (project uses `add_subdirectory` on Boost)

On Windows, set before configuring:
```bash
set BOOST_ROOT=C:\development\boost_1_88_0
```

Submodules must be initialized:
```bash
git submodule update --init
```

## Build Commands

**Configure and build** (use CMake presets from `CMakePresets.json`):

Windows (prefer Clang):
```bash
cmake --preset windows-clang-debug
cmake --build --preset windows-clang-debug
```

Linux:
```bash
cmake --preset ubuntu-gcc-debug
cmake --build --preset ubuntu-gcc-debug
```

Release builds:
```bash
cmake --preset windows-clang-release
cmake --build --preset windows-clang-release
```

## Testing

**Run all tests:**
```bash
ctest --preset windows-clang-debug --output-on-failure
```

**Run specific test(s) by regex:**
```bash
ctest -R <test_name_regex> --output-on-failure --preset windows-clang-debug
```

**Run with ThreadSanitizer (TSAN)** on Linux:
```bash
cmake --preset ubuntu-gcc-tsan
cmake --build --preset ubuntu-gcc-tsan
cd out/build/ubuntu-gcc-tsan/bin
setarch -R ./unit_tests
```

With TSAN filter:
```bash
setarch -R ./unit_tests --gtest_filter="DiscoveryClientLifecycleTest.*:DiscoveryServerLifecycleTest.*"
```

Note: CTest discovery is disabled for TSAN builds (`ENABLE_CTEST=OFF`).

**Single test binary location:**
- `out/build/<preset-name>/bin/unit_tests`

Tests use GoogleTest and are defined in `tests/unit_tests/`. Test discovery uses `gtest_discover_tests` when `ENABLE_CTEST` is ON.

## Coding Conventions

- **Types** (classes/structs/enums): `PascalCase` (e.g., `TcpClient`, `UdpDiscoveryServer`)
- **Functions, variables**: `snake_case` (e.g., `recv_buffer_size`, `async_connect`)
- **Private members**: prefix with `_` (e.g., `_io_context`, `_mutex`)
- **Macros**: `CAPITAL_WITH_UNDERSCORES` (use sparingly; prefer `constexpr`)
- **Acronyms**: Treat as words (e.g., `TcpClient`, not `TCPClient`)
- **Abbreviations**: Avoid except for well-known acronyms
- **Language**: C++17 only
- **Comments**: Only for non-obvious design decisions; use Javadoc-style Doxygen for public APIs
- **Includes**: Use project-relative paths: `#include "eestv/net/connection/tcp_client.hpp"`
- **Headers**: Use `#pragma once`; forward declare where possible to reduce compile times
- **Error handling**: Use RAII; prefer `std::optional`/`std::variant` over sentinels; avoid exceptions for control flow

## Architecture Notes

**State management:** Network classes (TcpClient, TcpServer, TcpConnection, UdpDiscoveryClient/Server) use dedicated state enums (e.g., `TcpClientStates`, `DiscoveryStates`) and thread-safe state transitions via mutexes. Check state headers (`*_states.hpp`) for lifecycle details.

**Async patterns:** All network I/O is async using Boost.Asio. Classes take an `io_context` reference and provide callback-based APIs (e.g., `async_connect`, `async_stop`). Synchronous stop methods are also provided for cleanup.

**Serialization:** The `Serializer`/`Deserializer` classes use `operator&` (Boost.Serialization-style) for chaining. Allowed types are enforced via `allowed_types.hpp`. Serialization writes directly to `LinearBuffer` via `get_write_head()`/`commit()` for zero-copy efficiency. Multiplexer/demultiplexer classes handle tagged message routing.

**LinearBuffer:** A non-circular buffer that resets to position 0 when empty, guaranteeing contiguous data layout. Used for serialization output and network send buffers. API: `get_read_head()`, `consume()`, `get_write_head()`, `commit()`.

**Thread safety:** Classes use `std::mutex` for state protection. Private members prefixed with `_` typically require lock acquisition. Flags classes provide thread-safe boolean operations.

**Testing:** Tests in `tests/unit_tests/` mirror the `code/eestv/` structure. Use GoogleTest. Lifecycle tests verify state transitions; integration tests verify end-to-end behavior (e.g., discovery client/server communication).

## Examples

Examples in `examples/` demonstrate library usage:
- `examples/net/discovery/`: UDP discovery client/server examples
- `examples/multi_serializer/`: Serialization multiplexer usage
- See individual example READMEs for details
