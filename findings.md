# DualSense SDK Productization Findings

This file records the current audit findings for the productization pass.

## Current Repository Inventory

- Public API: `include/dualsense/dualsense.h` exposes a C ABI with opaque `ds5_context`, `ds5_device`, and `ds5_audio_capture` handles; `include/dualsense/dualsense.hpp` provides a thin RAII C++ wrapper.
- Core source: `src/core.cpp`, `src/hid_backend_win.cpp`, `src/report_parser.cpp`, `src/output_report.cpp`, and `src/audio_wasapi_win.cpp`.
- Consumer-facing utilities: `tools/dualsense_diag.cpp` is the main operational validator; `tools/dualsense_ship_demo.cpp` is an integrated gameplay demo.
- Samples: C and C++ samples exercise enumeration/open/poll/output basics; C# sample only covers init/shutdown.
- Docs: `docs/user-guide.md`, `docs/protocol-notes.md`, and `docs/ship-demo-test-steps.md` already describe build, feature matrix, diagnostics, and USB-first transport policy.
- Build: CMake builds shared/static libraries, tools, samples, and one `dualsense_tests` executable. Existing `build-vs2026` CTest baseline passed on 2026-05-13.

## Productization Readiness Snapshot

- Status: useful Windows USB-first v1 SDK prototype / internal beta, not yet a packaged product SDK.
- Rough readiness: 60-65 percent for a developer-preview SDK; 35-45 percent for a productized SDK that third-party programs can consume without repo knowledge.
- Strong areas: stable-looking C ABI shape, capability flags, USB output controls, raw-report escape hatch, WASAPI endpoint helpers, diagnostics, real sample code, and a substantial regression test file.
- Weak areas: packaging/install metadata, ABI/versioning policy, richer language bindings, async/nonblocking patterns, production-grade error model, API reference generation, hardware test matrix, and separation between SDK tests and demo/game tests.

## Public API Audit

- Good: C ABI first with opaque handles is the right boundary for Windows SDK reuse and bindings.
- Good: structures include `size` and `version`, which can support ABI evolution.
- Gap: current validation only checks struct size; `version` is documented but not enforced or used for compatibility branching.
- Gap: `ds5_get_last_error()` is thread-local global state, not context/device scoped. This is simple but fragile for host apps using multiple SDK contexts or callbacks.
- Gap: C++ wrapper does not cover audio APIs or endpoint enumeration, and does not expose ergonomic capability checks.
- Gap: trigger API compresses multiple DualSense trigger modes into a small struct without named builder helpers; callers must know field semantics.
- Gap: no explicit `ds5_result_to_string`, no version query like `ds5_get_version`, and no compile/runtime ABI compatibility helpers.
- Gap: `ds5_set_haptic_pattern` blocks the caller thread for the requested duration, which is surprising for gameplay or UI loops.

## Implementation Audit

- Good: USB vs Bluetooth transport behavior is explicit and conservative.
- Good: output state is cached per device and guarded by a mutex before writing current output report.
- Good: report parser preserves raw input bytes for diagnostics and protocol experiments.
- Risk: `ds5_poll_state` uses blocking `ReadFile` on a synchronous handle. A product SDK likely needs timeout/nonblocking polling or overlapped I/O.
- Risk: enumeration opens every HID path with read/write access to identify attributes; this is workable for diagnostics but may skip devices when access is denied by another process.
- Risk: audio playback/capture are useful helpers but currently synchronous/polling based and focused on simple PCM; production use needs clearer lifetime, callback error reporting, and format negotiation behavior.
- Risk: duplicated helper code exists (`copy_string`, WAV loading in diag and demo), which is acceptable for tools but should not leak into library design.
- Risk: tests use `assert`, not a test framework with named failures, fixtures, or CI-friendly reporting.

## Docs, Samples, Diagnostics, Tests

- Strong: user guide has English and Chinese sections, build steps, feature matrix, diagnostics, and hardware notes.
- Strong: `dualsense_diag` covers practical validation paths that the ship demo does not.
- Gap: no dedicated API reference generated from headers, no changelog/release notes, no package consumption guide, and no support policy document.
- Gap: C# binding is only a minimal P/Invoke smoke sample. A product SDK needs real struct declarations, safe-handle wrappers, and examples for polling/output/audio.
- Gap: samples do not demonstrate nonblocking/game-loop polling, capability-gated output setup, or audio endpoint selection in small reusable code.
- Gap: tests mix library behavior with ship-demo gameplay behavior. Product SDK tests should be split into ABI/parser/output/audio units and demo-specific tests.
