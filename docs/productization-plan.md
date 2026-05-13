# DualSense SDK Productization Plan

## Executive Assessment

The project is past toy prototype stage. It already has a reasonable SDK spine: C ABI, C++ RAII wrapper, USB HID input/output, WASAPI audio helpers, diagnostics, samples, tests, and a real DirectX demo. It is currently suitable for controlled local use and developer-preview experiments.

It is not yet product-ready as a reusable component library for broad third-party adoption. The main gap is not raw feature count; it is SDK polish: stable ABI rules, packaging, install/consume flow, clearer header ergonomics, nonblocking I/O, richer bindings, better test separation, and reference-grade documentation.

Estimated readiness:

| Target | Readiness | Reason |
| --- | ---: | --- |
| Internal prototype | 80% | Core paths are implemented and tested locally. |
| Developer-preview SDK | 60-65% | API is usable, docs exist, but rough edges remain. |
| Product SDK | 35-45% | Packaging, ABI policy, async I/O, bindings, CI, and hardware matrix are incomplete. |

## Current Strengths

- C ABI is the right product boundary. It keeps the SDK callable from C, C++, C#, Rust, Python FFI, and game engines.
- Opaque handles (`ds5_context`, `ds5_device`, `ds5_audio_capture`) give room to change internals without breaking callers.
- Capability flags make USB/Bluetooth limits explicit instead of hiding unsupported behavior.
- `dualsense_diag` is a strong operational tool for enumeration, capabilities, polling, output reports, audio playback, capture, and raw output checks.
- Existing tests cover ABI struct basics, report parsing, output report encoding, transport capability policy, and demo control systems.
- Documentation already explains USB-first scope, diagnostics, and hardware caveats in English and Chinese.

## Product Gaps

### API And Header Surface

- `version` fields are documented but not yet used for compatibility checks or migration behavior.
- `ds5_get_last_error()` is thread-local global state; host applications may prefer context-scoped or explicit error buffers.
- C++ wrapper is incomplete compared with the C API, especially for audio endpoint enumeration/playback/capture.
- Trigger effects are exposed as raw fields. The API needs named builders/helpers for common modes so callers do not have to memorize byte semantics.
- No runtime version query, ABI compatibility query, or `ds5_result_to_string`.
- No explicit nonblocking poll or timeout poll API.
- `ds5_set_haptic_pattern` blocks the caller thread for `duration_ms`; that should become a helper layered above nonblocking primitives, or be documented as blocking.

### Implementation

- HID reads are synchronous and can block. Product use needs timeout or overlapped I/O.
- Enumeration may skip devices if read/write open fails. A product SDK should separate discovery from access testing and report access state clearly.
- WASAPI helpers work for simple PCM paths but need clearer format negotiation, callback error reporting, and endpoint selection contracts.
- Some utility logic is duplicated across tools, especially WAV parsing and string copying.
- Internal protocol constants need more named documentation at the implementation boundary.

### Tests And Verification

- Tests currently use `assert`; failures are less useful than named test cases from a lightweight framework.
- Library tests and ship-demo gameplay tests are mixed in one executable.
- No CI matrix is defined.
- No hardware verification checklist is tied to releases.
- No ABI compatibility test compares header size/layout across releases.

### Packaging And Adoption

- CMake installs headers and targets, but there is no package config/version file for `find_package`.
- No release layout is defined: include/lib/bin/samples/docs.
- No NuGet/vcpkg/FetchContent guidance.
- C# sample is only init/shutdown; it is not a real binding example.
- No generated API reference, changelog, support policy, or known limitations page.

## Development Plan

### Milestone 1: SDK Boundary Cleanup

Status: partially implemented on 2026-05-13. Runtime version helpers, result-code strings, public struct initializers, trigger builders, C++ wrapper coverage, public header comments, focused tests, and the `0.2.0` version bump have landed. Remaining work in this milestone is mostly around broader binding polish and deciding whether to add context-scoped error buffers.

Goal: make the public surface easier to consume without changing the core design.

- Add `ds5_get_version`, `ds5_result_to_string`, and `ds5_get_last_error_message(ds5_context*, ...)` or an explicit fixed-buffer error query.
- Enforce `version == DS5_STRUCT_VERSION` for v1 structs, or document and implement a forward-compatible version policy.
- Add small C helper initializers such as `ds5_device_info_init`, `ds5_state_init`, `ds5_audio_format_init`, and `ds5_trigger_effect_init`.
- Add trigger-effect helper constructors:
  - `ds5_trigger_effect_off`
  - `ds5_trigger_effect_constant_resistance`
  - `ds5_trigger_effect_section_resistance`
  - `ds5_trigger_effect_weapon`
  - `ds5_trigger_effect_vibration`
- Extend the C++ wrapper to cover endpoint enumeration, playback, capture lifetime, capability checks, and trigger builders.
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
- Replace raw `assert` with a small test framework or local test macros that print test names and expected values.
- Add CTest labels: `unit`, `sdk`, `demo`, `hardware`.
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

- Add `dualsenseConfig.cmake` and `dualsenseConfigVersion.cmake`.
- Define release artifacts:
  - `include/`
  - `bin/dualsense.dll`
  - `lib/dualsense.lib`
  - `lib/dualsense_static.lib`
  - `samples/`
  - `docs/`
- Add a minimal installed-consumer sample project that uses `find_package(DualSense CONFIG REQUIRED)`.
- Add vcpkg or FetchContent instructions.
- Expand C# P/Invoke sample into a real binding sample with structs, enums, SafeHandle-style ownership, enumeration, polling, and output.
- Add a `CHANGELOG.md` and semantic versioning policy.

Acceptance:

- A clean external CMake project can consume an installed SDK.
- C# sample can enumerate and poll a real controller.
- Release zip has predictable layout and smoke-test instructions.

### Milestone 5: Documentation And Comments

Goal: document what matters at the right layer without adding noisy comments.

- Add Doxygen-style comments to `include/dualsense/dualsense.h` for every public type and function.
- Document ownership, nullability, blocking behavior, transport support, thread-safety, and lifetime ordering.
- Add comments near protocol constants and report offsets in `report_parser.cpp` and `output_report.cpp`.
- Keep comments out of obvious implementation code; focus comments on protocol facts, Windows API quirks, and caller contracts.
- Generate API reference from the public header.
- Add `docs/known-limitations.md` covering Bluetooth output, endpoint availability, firmware, and audio route assumptions.

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
- Avoid repeated endpoint enumeration in hot paths; document endpoint caching.
- Reuse buffers for polling/audio paths where practical.
- Keep raw report copies bounded and optional if future profiling shows pressure.

Acceptance:

- Common frame-loop use can update multiple feedback channels with one HID write.
- Polling can be bounded by caller-chosen timeout.
- No performance-sensitive API requires repeated heap allocation in the normal path.

## Recommended Next Slice

The best next implementation slice is Milestone 1 plus the low-risk part of Milestone 5:

1. Add public C helper initializers, version/result helpers, and trigger builders.
2. Extend C++ wrapper to cover those helpers and audio enumeration/playback.
3. Add focused tests for the new API surface.
4. Add Doxygen comments to the public header for ownership, blocking behavior, and transport support.

This slice improves caller ergonomics immediately without changing the HID protocol core.
