# Packaging And Consumer Workflow

This SDK installs a CMake package named `DualSense`.

## Build And Install

From the repository root:

```powershell
cmake -S . -B build-vs2026 -G "Visual Studio 17 2022" -A x64
cmake --build build-vs2026 --config Release
cmake --install build-vs2026 --config Release --prefix C:\SDKs\DualSense
```

The install tree contains:

- `include/dualsense/`: public C and C++ headers.
- `lib/`: static library, import library, and CMake package files.
- `bin/`: shared library runtime when `DS5_BUILD_SHARED=ON`.
- `share/doc/dualsense_sdk/`: README, changelog, and docs.
- `share/dualsense/samples/`: C, C++, and C# samples.

## Consume From CMake

Configure a downstream project with `CMAKE_PREFIX_PATH` pointing at the install prefix:

```powershell
cmake -S examples\cmake-consumer -B build-consumer -DCMAKE_PREFIX_PATH=C:\SDKs\DualSense
cmake --build build-consumer --config Release
```

Consumer CMake example:

```cmake
find_package(DualSense CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE DualSense::dualsense_static)
```

When using the shared library target, make sure `dualsense.dll` is next to the executable or on `PATH`.

## Versioning

The runtime version is available through:

- `ds5_get_version`
- `ds5_get_version_string`

The installed CMake package also provides `dualsenseConfigVersion.cmake`, so consumers can request a compatible SDK version:

```cmake
find_package(DualSense 0.2 CONFIG REQUIRED)
```
