# Release Checklist

Use this checklist before publishing a developer-preview SDK package.

## Build And Unit Tests

```powershell
cmake -S . -B build-vs2026 -G "Visual Studio 18 2026" -A x64
cmake --build build-vs2026 --config Release
ctest --test-dir build-vs2026 -C Release --output-on-failure
```

Also run the Debug configuration when validating recent code changes:

```powershell
cmake --build build-vs2026 --config Debug
ctest --test-dir build-vs2026 -C Debug --output-on-failure
```

Expected result:
- All library, sample, tool, demo, and test targets build.
- `dualsense_tests` passes and prints named test failures if anything breaks.

## Install And Consumer Smoke Test

```powershell
cmake --install build-vs2026 --config Release --prefix C:\SDKs\DualSense
cmake -S examples\cmake-consumer -B build-consumer -G "Visual Studio 18 2026" -A x64 -DCMAKE_PREFIX_PATH=C:\SDKs\DualSense
cmake --build build-consumer --config Release
```

Expected result:
- `find_package(DualSense CONFIG REQUIRED)` resolves from the install prefix.
- The external consumer executable links against `DualSense::dualsense_static`.

Windows note:
- If Visual Studio configure fails with duplicate `Path` / `PATH`, rerun from a clean Developer Command Prompt or a child process with only one process-environment path key.
- Use an absolute `CMAKE_PREFIX_PATH`; relative install prefixes are easy to resolve from the wrong working directory.

## Hardware Smoke Test

Connect a DualSense over USB before running these checks.

List devices and endpoints:

```powershell
.\build-vs2026\Release\dualsense_diag.exe
```

Expected result:
- One USB DualSense is listed.
- Capabilities include input, lightbar, player LEDs, mic LED, classic rumble, haptics, adaptive triggers, speaker, microphone, headset jack, touchpad, IMU, and raw reports.
- Speaker and microphone endpoints appear when Windows exposes them.

Poll input:

```powershell
.\build-vs2026\Release\dualsense_diag.exe --poll-timeout 120 16
```

Expected result:
- Sticks, triggers, buttons, touch, and IMU values update when the controller is moved.

Check feedback:

```powershell
.\build-vs2026\Release\dualsense_diag.exe --test
```

Expected result:
- Lightbar, player LEDs, mic LED, rumble, and adaptive triggers visibly/audibly respond.
- Feedback is reset at the end.

Check speaker playback:

```powershell
.\build-vs2026\Release\dualsense_diag.exe --tone 1500
```

Expected result:
- Sound is heard from the controller speaker endpoint.

Check microphone capture:

```powershell
.\build-vs2026\Release\dualsense_diag.exe --capture 3000
```

Expected result:
- Capture reports nonzero bytes when Windows exposes the controller microphone and privacy settings allow access.

## Documentation

- README build commands match the intended release generator/configuration.
- `CHANGELOG.md` has an entry for the release.
- `docs/known-limitations.md` matches the actual feature scope.
- `docs/packaging.md` describes the install layout and CMake consumer flow.
- Hardware test results are recorded with controller transport, VID/PID, capabilities, and audio endpoint names.
