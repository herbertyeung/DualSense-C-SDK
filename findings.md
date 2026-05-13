# Findings

## Current Project State

- Branch: `feature/sdk-packaging-consumer-workflow`.
- Working tree was clean at plan creation.
- Latest commits show SDK packaging, bounded polling APIs, product API cleanup, and speaker routing work already landed.
- Project is currently a Windows USB-first developer-preview SDK, not a full transport-complete DualSense stack.

## Implemented Strengths

- Public C ABI and C++ RAII wrapper exist.
- `dualsense_diag` is the primary operational verification tool.
- `dualsense_ship_demo` provides an integrated DirectX sample.
- Nonblocking and timeout polling APIs are implemented.
- Persistent overlapped HID input reads are implemented for opened runtime handles.
- Packaging and installed CMake consumer flow exist.
- Public header comments already cover many ownership, blocking, and transport contracts.

## Maintenance Hotspots

- `tools/dualsense_ship_demo.cpp`: about 1064 lines. Highest refactor candidate, but rendering/demo changes need careful behavior preservation.
- `tests/test_main.cpp`: about 724 lines. Best first refactor candidate because better test names and separation reduce risk for later changes.
- `tools/dualsense_diag.cpp`: about 510 lines. Useful but should not be split before tests unless needed.
- `src/audio_wasapi_win.cpp`: about 430 lines. Windows API behavior deserves comments more than broad refactor.
- `tools/ship_systems.h`: about 430 lines. Contains target lock, motion calibration, bullets, camera, and feedback-zone logic.

## Algorithm Candidates

- Target lock currently computes distance and angle using `sqrt` and `acos` per target. This can likely be converted to squared-distance and cosine-threshold comparisons while preserving behavior.
- Demo frame-loop work should avoid repeated endpoint/device queries and repeated allocations.
- Motion calibration has useful behavior but is dense; refactor should focus on naming intermediate concepts and adding tests before changing math.

## Comment Candidates

- `src/report_parser.cpp`: report IDs, offsets, IMU layout, touch point packing, and Bluetooth short report limitations.
- `src/output_report.cpp`: USB output report length, valid flags, trigger byte layout, player LED mask, and lightbar bytes.
- `src/audio_wasapi_win.cpp`: endpoint matching assumptions, shared-mode format negotiation, drain-before-stop behavior, and controller speaker route assumptions.

## Productization Note

`docs/productization-plan.md` contains some stale gap wording now that 0.2 work has landed. It should be updated during the documentation phase so it reflects current state rather than old missing items.

## Phase 1 Baseline Results

- `cmake --build build-vs2026 --config Debug` passed.
- `ctest --test-dir build-vs2026 -C Debug --output-on-failure` passed with 1 test target.
- `cmake --install build-vs2026 --config Debug --prefix build-vs2026\install-sdk` passed.
- External consumer configure initially failed because the process environment had duplicate `Path` and `PATH` keys. MSBuild reported: `System.ArgumentException: Item has already been added. Key in dictionary: 'Path' Key being added: 'PATH'`.
- External consumer configure also failed when `CMAKE_PREFIX_PATH` was relative. Using a deduplicated environment plus absolute install prefix fixed configure.
- External consumer build passed in `build-vs2026\consumer-clean-env-abs`.
- `dualsense_diag` detected one USB controller: VID `0x054c`, PID `0x0ce6`, transport `1`, product `DualSense Wireless Controller`, capabilities `0x1fff`.
- `dualsense_diag` detected two audio endpoints: `Speakers (DualSense Wireless Controller)` and `Headset Microphone (DualSense Wireless Controller)`.

## Phase 2 Direction

The current test target is one large executable built from `tests/test_main.cpp`. The next change should either split that file by responsibility or add a local named-test harness first, so future protocol and algorithm changes produce actionable failure messages.

## Phase 2 Results

- Added a minimal local test harness in `tests/test_main.cpp` instead of splitting files immediately.
- This kept the behavioral coverage unchanged while improving failure output.
- Direct test execution now prints one `PASS <test-name>` line per test.
- CTest labels were added to the aggregate test target. Because the executable is still aggregate, labels currently describe coverage areas rather than separate executables.
- A later split can now be mechanical: move named tests into dedicated files while keeping the same `run_test` pattern.

## Phase 3 Results

- `src/output_report.cpp` now names the USB output report byte offsets used for report id, valid flags, rumble, mic LED, trigger blocks, player LEDs, and lightbar.
- `src/report_parser.cpp` now names short Bluetooth report offsets and extended input payload offsets.
- Protocol comments were added only where the layout is non-obvious: trigger block size, Bluetooth short-report limitations, and IMU packing.
- Parser/output behavior stayed covered by existing tests.

## Phase 4 Results

- `tools/dualsense_diag.cpp` and `tools/dualsense_ship_demo.cpp` had duplicate PCM16 WAV parsing.
- Shared parser now lives in `tools/wav_pcm.cpp` / `tools/wav_pcm.h`.
- `dualsense_diag`, `dualsense_ship_demo`, and `dualsense_tests` all link the shared parser.
- The test target now runs from the source directory so asset-backed tests can resolve `assets/commander_ready.wav`.

## Phase 5 Progress

- `ds5_demo_find_lock_target` no longer computes full distance/angle for every target before filtering.
- It now rejects by squared distance first and by cosine threshold second, then computes angle/distance only for accepted scoring candidates.
- Added a regression test for a close target outside the lock cone.
- Ship demo now reuses CPU-side scratch vertex vectors for feedback zones, engine flame, and light bullets instead of allocating those vectors in every render call.
- Direct3D vertex buffers are still rebuilt per frame for dynamic debug geometry. Reusing D3D dynamic buffers would be a larger rendering change and should be handled separately if profiling shows it matters.
- Motion calibration behavior was preserved while moving stillness range updates, quiet-window checks, and bias blending into named helper methods.

## Phase 6 Results

- Added `docs/known-limitations.md` for current v1 scope limits.
- Added `docs/release-checklist.md` with build, CTest, install, consumer, and USB hardware smoke checks.
- Updated README documentation links and `CHANGELOG.md`.
- Updated `docs/productization-plan.md` to reflect current implemented work: bounded polling, overlapped input, CMake package config, named test harness, CTest labels, shared WAV parser, known limitations, and release checklist.
- Install verification copied the new docs into `build-vs2026\install-sdk\share\doc\dualsense_sdk`.
- Final `dualsense_diag` hardware smoke detected one USB DualSense with `capabilities=0x1fff`, plus speaker and microphone endpoints.
