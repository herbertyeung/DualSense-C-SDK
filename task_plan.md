# DualSense SDK 0.3 Goal Plan

## Goal

Move the project from a usable Windows USB-first developer preview toward a maintainable SDK release by improving code structure, protocol comments, and runtime algorithms without expanding the feature surface.

Target name: `0.3 maintainability and runtime polish`.

The goal is intentionally not "add more DualSense features". The current code already has a working C ABI, C++ wrapper, diagnostics, samples, packaging, nonblocking polling, audio helpers, and a DirectX ship demo. The next useful step is to make the implementation easier to change, easier to verify, and less likely to regress.

## Success Criteria

- Core SDK behavior remains unchanged for existing public APIs.
- Unit tests are easier to diagnose than raw `assert` failures.
- Protocol constants and report offsets are named and commented at the implementation boundary.
- Large tool/demo code is split only where it reduces real maintenance cost.
- Hot-path algorithms avoid unnecessary work in normal frame loops.
- Verification includes build, CTest, installed consumer smoke test, and a short hardware checklist when a USB controller is available.

## Scope

In scope:
- Refactor internal code organization.
- Add necessary comments around public contracts, Windows API quirks, protocol offsets, and non-obvious algorithms.
- Optimize algorithms in input/demo loops where there is a clear cost or clarity win.
- Improve tests and diagnostics separation.

Out of scope for this goal:
- Full Bluetooth output support.
- New platform support.
- New language bindings beyond keeping the existing samples healthy.
- Large API redesign unless a current API blocks the refactor.

## Phase 1: Baseline And Risk Map

Status: complete

Tasks:
- Build the current branch and run `ctest`.
- Confirm installed CMake consumer flow still works.
- Record current hardware availability and controller transport if a DualSense is connected.
- Identify top maintenance risks by file size and coupling.

Verify:
- `cmake --build build-vs2026 --config Debug`
- `ctest --test-dir build-vs2026 -C Debug --output-on-failure`
- External CMake consumer builds from installed SDK.

Result:
- Main build passed on 2026-05-13.
- CTest passed: 1/1 `dualsense_tests`.
- Install step passed to `build-vs2026\install-sdk`.
- External CMake consumer configure/build passed when run with a deduplicated `Path` environment and an absolute `CMAKE_PREFIX_PATH`.
- USB DualSense was detected by `dualsense_diag` with `capabilities=0x1fff`, plus render and capture audio endpoints.

## Phase 2: Test Refactor

Status: complete

Tasks:
- Split `tests/test_main.cpp` into focused test files or a local named-test harness.
- Preserve the same behavioral coverage first.
- Add CTest labels for SDK, parser, output, demo logic, and hardware/manual checks.

Verify:
- Failures print the test name and expected condition.
- CI-friendly tests do not require a physical controller.

Result:
- Added a local named-test harness to `tests/test_main.cpp`.
- Each test now runs through `run_test(name, function)` and prints `PASS <name>`.
- Failed expectations throw a test failure that includes the test name, expression, file, and line.
- Added CTest labels to the current aggregate target: `unit;sdk;parser;output;demo`.
- `dualsense_tests` builds and passes.

## Phase 3: Protocol Comments And Constants

Status: complete

Tasks:
- Replace magic report indexes in `src/report_parser.cpp` and `src/output_report.cpp` with named constants.
- Add concise comments for DualSense report layout facts, not obvious C++ operations.
- Keep public header comments focused on ownership, blocking behavior, transport support, and lifetime order.

Verify:
- Parser and output tests still pass.
- A reader can trace each report byte used by parser/output code.

Result:
- Named USB output report size, report id, valid flags, rumble, mic LED, trigger, player LED, and lightbar byte offsets.
- Named input report IDs, Bluetooth short-report offsets, extended payload offsets, IMU offsets, and touch point packing offsets.
- Added focused protocol comments for trigger blocks, Bluetooth short input reports, and IMU sample layout.
- `dualsense_tests` passed after the changes.

## Phase 4: Tool And Demo Structure Refactor

Status: complete

Tasks:
- Extract reusable WAV loading out of `tools/dualsense_ship_demo.cpp` if diagnostics and demo need the same logic.
- Keep ship demo control, feedback, motion, and rendering responsibilities separated.
- Avoid speculative abstractions around DirectX rendering unless they reduce the current 1000-line demo maintenance burden.

Verify:
- Ship demo still builds.
- Existing ship control and feedback tests still pass.
- Behavior and accepted control semantics stay the same.

Result:
- Extracted duplicated PCM16 WAV parsing from `dualsense_diag` and `dualsense_ship_demo` into `tools/wav_pcm.cpp` / `tools/wav_pcm.h`.
- Added the shared WAV parser to diagnostic, ship demo, and test targets.
- Added a test that loads `assets/commander_ready.wav` and verifies the expected PCM16 format.
- `dualsense_diag`, `dualsense_ship_demo`, and `dualsense_tests` build successfully.

## Phase 5: Algorithm Optimization

Status: complete

Tasks:
- Optimize target lock scoring to avoid unnecessary `sqrt` and `acos` where cosine thresholds and squared distance are enough.
- Audit frame-loop allocations and repeated endpoint/device work.
- Keep motion calibration behavior stable while making bias/stillness logic easier to read.
- Consider a batched output-update API only if repeated HID writes are shown to be a real call-site problem.

Verify:
- Existing tests for target lock, motion calibration, and feedback behavior pass.
- Any algorithm change has a focused test that proves equivalent or deliberately improved behavior.

Progress:
- Optimized target-lock filtering to reject out-of-range targets with squared distance and reject out-of-angle targets with a cosine threshold before computing angle score.
- Added a focused test for a close target outside the lock angle losing to a farther in-angle target.
- Full Debug build and CTest pass.

Result:
- Reused ship-demo scratch vertex buffers for feedback zones, engine flame, and light bullets to avoid repeated CPU vector allocation in the render loop.
- Refactored gyro stillness calibration into named helper methods without changing thresholds, blend math, or public behavior.
- `dualsense_tests` and `dualsense_ship_demo` build successfully, and CTest passes.

## Phase 6: Documentation And Release Checklist

Status: complete

Tasks:
- Add or update known limitations for Bluetooth output, audio endpoint assumptions, firmware/cable caveats, and hardware validation.
- Update productization plan so completed 0.2 items are not listed as current gaps.
- Add a release checklist for build, unit tests, packaging, consumer build, and optional hardware smoke tests.

Verify:
- README, productization plan, and changelog agree on the project status.
- Manual hardware checklist has concrete commands and expected observations.

Result:
- Added `docs/known-limitations.md` for transport, audio endpoint, firmware, runtime, and testing limits.
- Added `docs/release-checklist.md` for build, package, consumer, and hardware smoke validation.
- Updated README documentation links and changelog.
- Updated `docs/productization-plan.md` so completed 0.2 and 0.3 work is no longer listed as an active gap.
- Install step confirmed the new docs are included under `share/doc/dualsense_sdk`.
- `dualsense_diag` still detects the connected USB controller with `capabilities=0x1fff` and both audio endpoints.

## Current Decision

All planned phases are complete. The next recommended slice is splitting the aggregate test executable into focused targets and adding CI for hardware-free unit tests.
