# Changelog

## Unreleased

- Added installed CMake package files for `find_package(DualSense CONFIG REQUIRED)`.
- Added an external CMake consumer example under `examples/cmake-consumer`.
- Expanded the C# P/Invoke sample to enumerate, open, poll with timeout, and reset feedback.
- Added packaging and downstream consumption documentation.

## 0.2.0

- Added bounded input polling with `ds5_poll_state_timeout`.
- Added nonblocking input polling with `ds5_try_poll_state`.
- Added `DS5_E_TIMEOUT` result code.
- Added `ds5_reset_feedback` to clear SDK-managed rumble, adaptive triggers, and mic LED with one output report.
- Switched opened runtime HID handles to persistent overlapped input reads so timeout and try polling do not cancel a read on every call.
- Added file author headers to SDK and demo header files.

## 0.1.0

- Initial developer-preview SDK surface for Windows USB DualSense support.
- Added C ABI, C++ RAII wrapper, diagnostics, samples, WASAPI audio helpers, and DirectX ship demo.
