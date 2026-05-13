# DualSense SDK Productization Plan

## Executive Assessment

The project is past toy prototype stage. It already has a reasonable SDK spine: C ABI, C++ RAII wrapper, USB HID input/output, WASAPI audio helpers, diagnostics, samples, tests, and a real DirectX demo. It is currently suitable for controlled local use and developer-preview experiments.

It is not yet product-ready as a reusable component library for broad third-party adoption. The main gap is not raw feature count; it is SDK polish: stable ABI policy, broader binding support, CI, hardware-matrix discipline, richer diagnostics around audio/capture failures, and reference-grade documentation.

Estimated readiness:

| Target | Readiness | Reason |
| --- | ---: | --- |
| Internal prototype | 80% | Core paths are implemented and tested locally. |
| Developer-preview SDK | 70-75% | API, docs, packaging, bounded polling, and diagnostics are usable, but verification and release discipline still need work. |
| Product SDK | 45-55% | Packaging and bounded polling have landed; ABI policy, bindings, CI, and hardware matrix are still incomplete. |

## Current Strengths

- C ABI is the right product boundary. It keeps the SDK callable from C, C++, C#, Rust, Python FFI, and game engines.
- Opaque handles (`ds5_context`, `ds5_device`, `ds5_audio_capture`) give room to change internals without breaking callers.
- Capability flags make USB/Bluetooth limits explicit instead of hiding unsupported behavior.
- `dualsense_diag` is a strong operational tool for enumeration, capabilities, polling, output reports, audio playback, capture, and raw output checks.
- Existing tests cover ABI struct basics, report parsing, output report encoding, transport capability policy, and demo control systems.
- Documentation already explains USB-first scope, diagnostics, and hardware caveats in English and Chinese.

## Product Gaps

### API And Header Surface

- `version` fields are checked for v1 public structs, but the project still needs a written long-term ABI migration policy.
- `ds5_get_last_error()` is thread-local global state; host applications may prefer context-scoped or explicit error buffers.
- C++ wrapper covers core C API paths, including audio endpoint enumeration/playback/capture lifetime, but it still needs broader examples and binding-polish review.
- Trigger effects have named builders, but the API still needs more usage guidance for common gameplay patterns.
- Runtime version and result-code helpers exist; a formal ABI compatibility query is still missing.
- Bounded and nonblocking polling APIs exist.
- `ds5_set_haptic_pattern` blocks the caller thread for `duration_ms`; that should become a helper layered above nonblocking primitives, or be documented as blocking.

### Implementation

- Opened runtime HID reads use persistent overlapped I/O with blocking, timeout, and nonblocking public polling APIs.
- Enumeration may skip devices if read/write open fails. A product SDK should separate discovery from access testing and report access state clearly.
- WASAPI helpers work for simple PCM paths but need clearer format negotiation, callback error reporting, and endpoint selection contracts.
- Shared PCM16 WAV parsing is reused by tools; remaining utility duplication should be handled case by case.
- Core parser/output protocol constants are now named; more Windows audio comments may still be useful.

### Tests And Verification

- Tests now use a local named-test harness, but library tests and ship-demo gameplay tests are still mixed in one executable.
- No CI matrix is defined.
- No hardware verification checklist is tied to releases.
- No ABI compatibility test compares header size/layout across releases.

### Packaging And Adoption

- CMake installs headers, targets, package config/version files, samples, and docs.
- A release layout is defined: include/lib/bin/samples/docs.
- No NuGet/vcpkg/FetchContent guidance.
- C# sample is only init/shutdown; it is not a real binding example.
- Changelog and known limitations exist; generated API reference and support policy are still missing.

## Development Plan

### Milestone 1: SDK Boundary Cleanup

Status: mostly implemented on 2026-05-13. Runtime version helpers, result-code strings, public struct initializers, trigger builders, C++ wrapper coverage, public header comments, focused tests, version validation, and the `0.2.0` version bump have landed. Remaining work in this milestone is mostly around broader binding polish and deciding whether to add context-scoped error buffers.

Goal: make the public surface easier to consume without changing the core design.

- Add `ds5_get_version`, `ds5_result_to_string`, and `ds5_get_last_error_message(ds5_context*, ...)` or an explicit fixed-buffer error query. Runtime version and result strings are implemented; context-scoped error buffers remain open.
- Enforce `version == DS5_STRUCT_VERSION` for v1 structs, or document and implement a forward-compatible version policy. Status: implemented for current v1 validation paths.
- Add small C helper initializers such as `ds5_device_info_init`, `ds5_state_init`, `ds5_audio_format_init`, and `ds5_trigger_effect_init`. Status: implemented.
- Add trigger-effect helper constructors:
  - `ds5_trigger_effect_off`
  - `ds5_trigger_effect_constant_resistance`
  - `ds5_trigger_effect_section_resistance`
  - `ds5_trigger_effect_weapon`
  - `ds5_trigger_effect_vibration`
- Extend the C++ wrapper to cover endpoint enumeration, playback, capture lifetime, capability checks, and trigger builders. Status: implemented.
- Keep `ds5_send_raw_output_report` available, but mark it as advanced and unsafe for normal gameplay code.

Acceptance:

- Existing C/C++ samples become shorter and require less manual struct setup.
- C++ wrapper can exercise every public C API except raw experimental paths.
- Tests cover helper initialization and version validation failures.

### Milestone 2: I/O And Runtime Robustness

Goal: remove blocking surprises and make runtime behavior predictable for host apps.

- Add `ds5_poll_state_timeout(device, timeout_ms, state)` or a nonblocking `ds5_try_poll_state`. Status: implemented on 2026-05-13.
- Consider overlapped HID I/O internally for product builds. Status: implemented for opened runtime HID handles on 2026-05-13 using persistent pending input reads.
- Split enumeration into:
  - discovery info
  - open capability/access result
  - transport/capability summary
- Add explicit reset API, for example `ds5_reset_feedback(device)`, so demos do not duplicate shutdown cleanup. Status: implemented on 2026-05-13.
- Replace blocking `ds5_set_haptic_pattern` with a clearly named blocking helper or move timed behavior to sample code.
- Add stronger audio capture error reporting through callback status or `ds5_audio_capture_get_status`.

Acceptance:

- A game loop can poll without risking an indefinite block.
- Feedback cleanup is one documented call.
- Audio capture startup failures can be diagnosed by the caller.

### Milestone 3: Tests, Diagnostics, And CI

Goal: separate SDK correctness from demo behavior and make failures actionable.

- Split tests into:
  - `dualsense_abi_tests`
  - `dualsense_report_tests`
  - `dualsense_output_tests`
  - `dualsense_audio_unit_tests`
  - `dualsense_ship_demo_tests`
- Replace raw `assert` with a small test framework or local test macros that print test names and expected values. Status: implemented with a local named-test harness.
- Add CTest labels: `unit`, `sdk`, `demo`, `hardware`. Status: partially implemented on the aggregate test target.
- Add hardware smoke checklist for USB controller release validation:
  - enumerate controller
  - capabilities show full USB flags
  - poll buttons/sticks/triggers/touch/IMU
  - lightbar/player LEDs/mic LED
  - rumble and adaptive triggers
  - speaker tone/WAV
  - microphone capture
  - reset feedback
- Add CI for compile and unit tests without hardware.

Acceptance:

- CI can validate parser/output/ABI without a controller.
- Manual release checklist covers real hardware behavior.
- Demo regressions no longer obscure core SDK failures.

### Milestone 4: Packaging And Consumer Workflow

Goal: make the library consumable without knowing this repo's internals.

- Add `dualsenseConfig.cmake` and `dualsenseConfigVersion.cmake`. Status: implemented.
- Define release artifacts:
  - `include/`
  - `bin/dualsense.dll`
  - `lib/dualsense.lib`
  - `lib/dualsense_static.lib`
  - `samples/`
  - `docs/`
- Add a minimal installed-consumer sample project that uses `find_package(DualSense CONFIG REQUIRED)`. Status: implemented.
- Add vcpkg or FetchContent instructions.
- Expand C# P/Invoke sample into a real binding sample with structs, enums, SafeHandle-style ownership, enumeration, polling, and output.
- Add a `CHANGELOG.md` and semantic versioning policy. Changelog exists; semantic versioning policy still needs more detail.

Acceptance:

- A clean external CMake project can consume an installed SDK.
- C# sample can enumerate and poll a real controller.
- Release zip has predictable layout and smoke-test instructions.

### Milestone 5: Documentation And Comments

Goal: document what matters at the right layer without adding noisy comments.

- Add Doxygen-style comments to `include/dualsense/dualsense.h` for every public type and function.
- Document ownership, nullability, blocking behavior, transport support, thread-safety, and lifetime ordering.
- Add comments near protocol constants and report offsets in `report_parser.cpp` and `output_report.cpp`. Status: implemented for core parser/output layouts.
- Keep comments out of obvious implementation code; focus comments on protocol facts, Windows API quirks, and caller contracts.
- Generate API reference from the public header.
- Add `docs/known-limitations.md` covering Bluetooth output, endpoint availability, firmware, and audio route assumptions. Status: implemented.

Acceptance:

- A developer can understand correct call order and blocking behavior from the header alone.
- Protocol offsets in parser/output code are traceable and less risky to modify.
- Docs state exactly which features are USB-only.

### Milestone 6: Performance And Efficiency

Goal: improve efficiency where it affects host apps, not by premature micro-optimization.

- Avoid blocking HID reads in app/game loops.
- Reduce repeated output writes by coalescing state updates or offering a batched output API:
  - begin update
  - set light/rumble/triggers
  - flush once
- Avoid repeated endpoint enumeration in hot paths; document endpoint caching. Status: ship demo caches the speaker endpoint at startup.
- Reuse buffers for polling/audio paths where practical. Status: ship demo reuses CPU-side dynamic geometry buffers; D3D dynamic buffer reuse remains future work.
- Keep raw report copies bounded and optional if future profiling shows pressure.

Acceptance:

- Common frame-loop use can update multiple feedback channels with one HID write.
- Polling can be bounded by caller-chosen timeout.
- No performance-sensitive API requires repeated heap allocation in the normal path.

## Recommended Next Slice

The best next implementation slice is Milestone 3 plus the remaining low-risk parts of Milestone 6:

1. Split the aggregate test executable into focused ABI, parser, output, audio utility, and demo-logic targets.
2. Keep the named-test harness so failures stay actionable.
3. Add CI for compile and unit tests without hardware.
4. Consider D3D dynamic-buffer reuse in the ship demo only if profiling shows the current per-frame debug-geometry buffers matter.

This slice improves verification and runtime polish without changing the HID protocol core.
