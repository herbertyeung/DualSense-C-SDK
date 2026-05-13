# Known Limitations

This SDK is a Windows USB-first developer-preview release. The behavior below is intentional for the current v1 scope unless noted otherwise.

## Transport

- USB is the full-capability transport.
- Bluetooth support is limited to discovery and input polling when Windows exposes a readable HID path.
- Bluetooth output control is not implemented in v1. Lightbar, player LEDs, mic LED, rumble, adaptive triggers, haptics, raw output reports, speaker playback, and microphone capture should be treated as USB-only paths.
- Capability flags should be checked before enabling optional features. Do not assume an enumerated Bluetooth device supports the same features as USB.

## Audio Endpoints

- Speaker playback and microphone capture use Windows MMDevice/WASAPI endpoints. The SDK does not make the DualSense the system default audio device.
- Endpoint availability depends on Windows, firmware, cable/port quality, privacy settings, and device state.
- If the speaker endpoint is missing, check Windows Sound settings and reconnect the controller over USB.
- If the microphone endpoint is missing or capture fails, check Windows microphone privacy permissions.
- `ds5_audio_play_pcm` accepts PCM data and performs simple shared-mode conversion when needed, but it is not a general audio engine.

## Firmware And Device State

- Firmware updates are outside the SDK. Use Sony PlayStation Accessories for firmware maintenance.
- Some controller state can survive process exit if a program terminates without cleanup. Applications should call `ds5_reset_feedback` before closing a device.
- `ds5_send_raw_output_report` is an advanced USB-only escape hatch. It can put the controller into states that typed SDK helpers do not track.

## Runtime And Threading

- Opened HID input uses overlapped reads. `ds5_poll_state` blocks until a report arrives; game loops should prefer `ds5_poll_state_timeout` or `ds5_try_poll_state`.
- Output state for one device is serialized internally, but applications should still avoid issuing conflicting feedback updates from multiple systems without an ownership rule.
- `ds5_set_haptic_pattern` is a blocking helper because it sleeps for the requested duration before clearing rumble.

## Testing Scope

- Unit tests cover parser, output report, ABI helper, and demo logic paths without requiring hardware.
- Hardware behavior still needs manual USB validation for each release.
- A successful process exit from an audio or feedback command is not enough by itself; speaker playback, microphone capture, rumble, trigger resistance, and LEDs require actual observation.
