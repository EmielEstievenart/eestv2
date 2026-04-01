# eestv_lib (superproject wrapper)

Minimal instructions to checkout and build.

Prerequisites
- C++17 compiler, CMake >= 3.21, Ninja (recommended)
- Boost source tree available and pointed to by BOOST_ROOT (project uses add_subdirectory on $BOOST_ROOT)

Quick start, after cloning

```bash
# init and update submodules
git submodule update --init --recursive
```

Configure & build (from repo root).
Since Boost is added with `add_subdirectory()`, point the project to the root of
the Boost source tree first.

On Windows:

```bash
    set BOOST_ROOT=C:\development\boost_1_88_0
```

After this, you can configure the project:

```bash
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
```

Run `slayerlog`:

```bash
out/build/windows-msvc-release/bin/slayerlog --file path/to/log.txt
```

Run tests

```bash
ctest --preset windows-msvc-release
```

If you prefer Clang on Windows:

```bash
cmake --preset windows-clang-debug
cmake --build --preset windows-clang-debug
ctest --preset windows-clang-debug
```

On Linux:

```bash
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release
ctest --preset linux-gcc-release
```

You can also run the full configure/build/test flow with a workflow preset:

```bash
cmake --workflow --preset windows-msvc-release
```

To run with TSAN use: 
setarch -R ./unit_tests

To run with a filter use: 
setarch -R ./unit_tests --gtest_filter="DiscoveryClientLifecycleTest.*:DiscoveryServerLifecycleTest.*:DiscoveryIntegrationTest.*"

CTEST no longer works with this enabled, that's why they are disabled with this config. 
