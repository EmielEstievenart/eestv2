# eestv_lib (superproject wrapper)

Minimal instructions to checkout and build.

Prerequisites
- C++17 compiler, CMake >= 3.21, Ninja (recommended)
- Boost source tree available and pointed to by BOOST_ROOT (project uses add_subdirectory on $BOOST_ROOT)

Quick start, after cloning

```bash
# init and update submodules
git submodule update --init
```

Configure & build (from repo root). 
Since boost is used, you need to point the project to the root of boost. 

On Windows:

```bash
    set BOOST_ROOT=C:\development\boost_1_88_0
```

After this, you can configure the project:

```bash
mkdir out\build
cd out\build
cmake -S ../.. -B . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

Run tests

```bash
cd out/build
ctest --output-on-failure -C Release
```

To run with TSAN use: 
setarch -R ./unit_tests

To run with a filter use: 
setarch -R ./unit_tests --gtest_filter="DiscoveryClientLifecycleTest.*"

CTEST no longer works with this enabled, that's why they are disabled with this config. 
