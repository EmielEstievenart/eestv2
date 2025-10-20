# AI Coding Guidelines for eestv_lib

## Building
- Use CMake and CMakePresets.txt. On Windows, prefer the preset with clang. 

## Coding Conventions
- Types: PascalCase (e.g., `UdpDiscoveryClient`)
- Everything else: snake_case (e.g., `recv_buffer_size`, `_io_context`)
- Only macros (not constexpr) in `CAPITAL_LETTERS_WITH_UNDERSCORES`
- Private members prefixed with `_`
- Avoid abbreviations; acronyms OK
- Comments only for non-obvious design decisions; use Javadoc Doxygen style
- Limit to C++17
