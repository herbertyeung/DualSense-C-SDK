# DualSense Ship Demo Test Steps

Build:

```powershell
cmake -S . -B build-vs2026 -G "Visual Studio 17 2022" -A x64
cmake --build build-vs2026 --config Debug --target dualsense_ship_demo dualsense_tests
ctest --test-dir build-vs2026 -C Debug --output-on-failure
```

Run:

```powershell
.\build-vs2026\Debug\dualsense_ship_demo.exe
```

Controls:

- Left stick: direct ship movement. X strafes left/right; Y moves forward/back. This does not use the R2/L2 throttle/brake path and does not create rumble by itself.
- Right stick: camera aim.
- L1/R1: roll.
- R2: main propulsion. Light press gives precision thrust, mid press cruises, deep press enters boost with stronger adaptive trigger resistance and right-side rumble.
- L2: brake/reverse feedback path, separate from left-stick movement and R2 propulsion.
- Cross: boost.
- Circle: dodge.
- Square: switch weapon event.
- Triangle/Touchpad: target lock and auto-follow are disabled while the demo runs in single-ship mode.
- Options: pause.
- Gyro Z: slight roll assist; fast flick triggers dodge when enabled.

Fallback:

- XInput controllers use Xbox-equivalent sticks/triggers/buttons.
- Keyboard fallback: arrows steer, I/J/K/L camera, A/D or Q/E roll, Space boost, Shift brake, T lock, Tab auto-follow, F fire, Z switch, C dodge, P pause.

Debug:

- The window title shows active input source, speed, trigger availability, gyro state, and last feedback event. Gyro ship control is off by default in `config/ship_controls.cfg`.
- Config values are loaded from `config/ship_controls.cfg`.
- If the DualSense speaker endpoint is available, lock/collision/switch/lost-target events play short system tones; audio failures degrade silently.

Coverage notes:

- Use `dualsense_ship_demo` for integrated gameplay input, motion, rumble, adaptive triggers, lightbar, and speaker-event behavior.
- Use `dualsense_diag --test` for the full current output API set: lightbar, player LEDs, mic LED, rumble, timed haptic helper, and all public trigger modes.
- Use `dualsense_diag --capabilities`, `--poll`, `--tone`, `--capture`, and `--raw-output-reset` for API paths that the ship demo does not exercise directly.
