# DualSense Component Library User Guide

## English

This component library provides a Windows C API and a small C++ RAII wrapper for finding, opening, reading, and driving a Sony PlayStation 5 DualSense controller. The v1 target is Windows 10/11 with the controller connected by USB for full output support.

### Supported Environment

- Platform: Windows 10/11.
- Build system: CMake 3.24 or newer.
- Compiler: MSVC or another Windows C/C++ toolchain supported by CMake.
- Controller: Sony PS5 DualSense.
- Best transport: USB. Bluetooth may enumerate and can be opened for input polling when Windows exposes a readable HID path, but v1 does not implement Bluetooth output control.

### Build

Configure and build with Visual Studio:

```powershell
cmake -S . -B build-vs2026 -G "Visual Studio 17 2022" -A x64
cmake --build build-vs2026 --config Debug
ctest --test-dir build-vs2026 -C Debug --output-on-failure
```

Configure and build with NMake:

```powershell
cmake -S . -B build-nmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build-nmake
ctest --test-dir build-nmake --output-on-failure
```

Useful build options:

| Option | Default | Meaning |
| --- | --- | --- |
| `DS5_BUILD_SHARED` | `ON` | Builds `dualsense.dll` in addition to the static library. |
| `DS5_BUILD_TESTS` | `ON` | Builds `dualsense_tests`. |
| `DS5_BUILD_SAMPLES` | `ON` | Builds `c_api_sample` and `cpp_sample`. |
| `DS5_BUILD_TOOLS` | `ON` | Builds `dualsense_diag` and `dualsense_ship_demo`. |

### API Overview

The C API is declared in `include/dualsense/dualsense.h`.

| Area | Main API | Notes |
| --- | --- | --- |
| Lifetime | `ds5_init`, `ds5_shutdown` | Create one context, then shut it down when all devices and audio captures are finished. |
| Logging/errors | `ds5_set_log_callback`, `ds5_get_last_error` | Most functions return `ds5_result`; call `ds5_get_last_error` after a failure. |
| Discovery | `ds5_enumerate` | First call with `devices = NULL` and `capacity = 0` to obtain the count. |
| Open/close | `ds5_open`, `ds5_close` | Open a `ds5_device_info` returned by enumeration. |
| Input | `ds5_poll_state` | Fills `ds5_state` with buttons, sticks, triggers, touch, IMU, battery, transport, and raw report bytes. |
| Capabilities | `ds5_get_capabilities` | Returns `ds5_capabilities.flags`; check feature flags before enabling UI or gameplay features. |
| LEDs | `ds5_set_lightbar`, `ds5_set_player_leds`, `ds5_set_mic_led` | Sets lightbar RGB, player LED mask, and mic LED mode. |
| Rumble/haptics | `ds5_set_rumble`, `ds5_set_haptic_pattern` | Classic rumble and a timed haptic helper. |
| Adaptive triggers | `ds5_set_trigger_effect` | Supports off, constant resistance, section resistance, weapon, and vibration modes. |
| Raw reports | `ds5_send_raw_output_report` | Advanced escape hatch for protocol experiments. |
| Audio | `ds5_audio_enumerate_endpoints`, `ds5_audio_play_pcm`, `ds5_audio_capture_start`, `ds5_audio_capture_stop` | Uses explicit Windows audio endpoint IDs and does not change the system default audio device. |

The C++ wrapper is declared in `include/dualsense/dualsense.hpp`.

- `DualSense::Context` owns `ds5_context`.
- `DualSense::Controller` owns `ds5_device`.
- `DualSense::Error` is thrown when a wrapped C call fails.
- `Controller::haptics()` exposes `rumble` and `pattern`.
- `Controller::triggers()` exposes `setEffect`, `setResistance`, and `off`.
- The wrapper also exposes lightbar, player LEDs, mic LED, raw output, capabilities, state polling, and feedback reset helpers.

For C structs that contain `size` and `version`, initialize both before passing the struct to the library:

```c
state.size = sizeof(state);
state.version = DS5_STRUCT_VERSION;
```

### Minimal C Example

```c
#include <dualsense/dualsense.h>
#include <stdio.h>

int main(void) {
  ds5_context* context = NULL;
  if (ds5_init(&context) != DS5_OK) {
    puts(ds5_get_last_error());
    return 1;
  }

  uint32_t count = 0;
  ds5_result result = ds5_enumerate(context, NULL, 0, &count);
  if (result != DS5_OK && result != DS5_E_INSUFFICIENT_BUFFER) {
    puts(ds5_get_last_error());
    ds5_shutdown(context);
    return 1;
  }

  printf("DualSense controllers found: %u\n", count);
  ds5_shutdown(context);
  return 0;
}
```

The repository version of this example is `samples/c_api_sample.c`.

### Minimal C++ Example

```cpp
#include <dualsense/dualsense.hpp>
#include <iostream>

int main() {
  try {
    DualSense::Context context;
    auto devices = context.enumerate();
    std::cout << "DualSense controllers found: " << devices.size() << "\n";
    if (!devices.empty()) {
      DualSense::Controller controller(context.native(), devices.front());
      controller.setLightbar(0, 64, 255);
      controller.haptics().rumble(32, 32);
      controller.triggers().setResistance(true, 16, 128);
      controller.resetFeedback();
    }
  } catch (const DualSense::Error& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
  return 0;
}
```

The repository version of this example is `samples/cpp_sample.cpp`.

### Feature Matrix

| Feature | USB | Bluetooth | Notes |
| --- | --- | --- | --- |
| Device enumeration | Yes | Limited | Bluetooth devices can be reported as reduced capability. |
| Open device for input polling | Yes | Limited | Bluetooth open depends on Windows exposing a readable HID path. |
| Open device for v1 output control | Yes | No | v1 returns `DS5_E_UNSUPPORTED_TRANSPORT` for Bluetooth output APIs. |
| Buttons, sticks, triggers | Yes | Limited | USB and short Bluetooth input reports are parsed. |
| D-pad | Yes | Limited | Reported through `ds5_state.dpad`. |
| Touchpad | Yes | Extended reports only | Exposed as two `ds5_touch_point` entries when available. |
| Gyro/accelerometer | Yes | Extended reports only | Exposed in `ds5_state.gyro_*` and `accel_*`. |
| Battery | Yes | Extended reports only | Exposed as `battery_percent`; charging state is not modeled in v1. |
| Lightbar | Yes | No for v1 Bluetooth | `ds5_set_lightbar`. |
| Player LEDs | Yes | No for v1 Bluetooth | `ds5_set_player_leds`; mask selects LEDs. |
| Mic LED | Yes | No for v1 Bluetooth | `DS5_MIC_LED_OFF`, `ON`, or `PULSE`. |
| Classic rumble | Yes | No for v1 Bluetooth | `ds5_set_rumble`. |
| Haptic pattern helper | Yes | No for v1 Bluetooth | `ds5_set_haptic_pattern`. |
| Adaptive triggers | Yes | No for v1 Bluetooth | `ds5_set_trigger_effect`. |
| Raw output reports | Yes | No for v1 Bluetooth | Advanced use only. |
| Speaker playback | Yes, via Windows audio endpoint | Not in v1 scope | Use explicit endpoint ID from `ds5_audio_enumerate_endpoints`. |
| Microphone capture | Yes, via Windows audio endpoint | Not in v1 scope | Use `ds5_audio_capture_start`. |
| Headset jack routing | Hardware/Windows endpoint dependent | Not in v1 scope | The library enumerates matching endpoints; it does not change Windows defaults. |

### Diagnostic and Demo Commands

After building, run these from the repository root. Adjust `Debug` and the build directory if you used another generator or configuration.

List controllers and audio endpoints:

```powershell
.\build-vs2026\Debug\dualsense_diag.exe
```

Run a short output test on the first controller:

```powershell
.\build-vs2026\Debug\dualsense_diag.exe --test
```

Print opened-device capabilities:

```powershell
.\build-vs2026\Debug\dualsense_diag.exe --capabilities
```

Poll input reports:

```powershell
.\build-vs2026\Debug\dualsense_diag.exe --poll 180
```

Play a tone on the first matching render endpoint:

```powershell
.\build-vs2026\Debug\dualsense_diag.exe --tone 1500
```

Capture from the first matching microphone endpoint for one second:

```powershell
.\build-vs2026\Debug\dualsense_diag.exe --capture 1000
```

Send a known USB raw output reset report:

```powershell
.\build-vs2026\Debug\dualsense_diag.exe --raw-output-reset
```

Run the ship demo:

```powershell
.\build-vs2026\Debug\dualsense_ship_demo.exe
```

The ship demo also supports XInput and keyboard fallback. See `docs/ship-demo-test-steps.md` for its controls and verification steps.

### Hardware Notes

- USB is the expected connection for the full feature set. Use a data-capable USB cable, not a charge-only cable.
- If enumeration works but output features fail, confirm the controller is connected over USB and not Bluetooth.
- Bluetooth support in v1 is intentionally conservative: discovery and input polling may work, but full output control is not enabled.
- Audio is handled through Windows MMDevice/WASAPI endpoints. The controller speaker, microphone, and headset jack appear as Windows audio endpoints when the OS and device expose them.
- The library never changes the Windows default playback or recording device. Select endpoint IDs explicitly.
- If the speaker or microphone does not appear, check Windows sound settings, privacy microphone permission, device firmware, and cable/port quality.
- Firmware updates are outside the library. Use Sony PlayStation Accessories for firmware maintenance.

### Error Handling Checklist

- Check every `ds5_result`.
- Log `ds5_get_last_error()` on failure.
- Initialize `size` and `version` fields before passing structs to the library.
- Check capability flags before using optional features.
- Reset rumble and trigger effects before exiting gameplay or diagnostics.
- Close devices before shutting down the context.

## 中文

本组件库提供 Windows 平台的 C API，以及一层轻量 C++ RAII 封装，用于发现、打开、读取和控制 Sony PlayStation 5 DualSense 手柄。v1 的完整能力目标是 Windows 10/11 + USB 连接的 DualSense 手柄。

### 支持环境

- 平台：Windows 10/11。
- 构建系统：CMake 3.24 或更新版本。
- 编译器：MSVC，或 CMake 支持的其他 Windows C/C++ 工具链。
- 手柄：Sony PS5 DualSense。
- 推荐连接：USB。Bluetooth 可能可以枚举出来，并且在 Windows 暴露可读 HID 路径时可用于输入轮询，但 v1 不实现 Bluetooth 输出控制。

### 构建

使用 Visual Studio 生成器：

```powershell
cmake -S . -B build-vs2026 -G "Visual Studio 17 2022" -A x64
cmake --build build-vs2026 --config Debug
ctest --test-dir build-vs2026 -C Debug --output-on-failure
```

使用 NMake：

```powershell
cmake -S . -B build-nmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build-nmake
ctest --test-dir build-nmake --output-on-failure
```

常用构建选项：

| 选项 | 默认值 | 含义 |
| --- | --- | --- |
| `DS5_BUILD_SHARED` | `ON` | 除静态库外，也构建 `dualsense.dll`。 |
| `DS5_BUILD_TESTS` | `ON` | 构建 `dualsense_tests`。 |
| `DS5_BUILD_SAMPLES` | `ON` | 构建 `c_api_sample` 和 `cpp_sample`。 |
| `DS5_BUILD_TOOLS` | `ON` | 构建 `dualsense_diag` 和 `dualsense_ship_demo`。 |

### API 概览

C API 声明在 `include/dualsense/dualsense.h`。

| 范围 | 主要 API | 说明 |
| --- | --- | --- |
| 生命周期 | `ds5_init`, `ds5_shutdown` | 创建一个 context；所有设备和音频采集结束后再关闭。 |
| 日志/错误 | `ds5_set_log_callback`, `ds5_get_last_error` | 大多数函数返回 `ds5_result`；失败后调用 `ds5_get_last_error` 获取说明。 |
| 发现设备 | `ds5_enumerate` | 先用 `devices = NULL`、`capacity = 0` 调一次获得数量。 |
| 打开/关闭 | `ds5_open`, `ds5_close` | 打开枚举返回的 `ds5_device_info`。 |
| 输入 | `ds5_poll_state` | 填充 `ds5_state`，包含按钮、摇杆、扳机、触摸、IMU、电量、传输类型和原始 report。 |
| 能力 | `ds5_get_capabilities` | 返回 `ds5_capabilities.flags`；启用 UI 或玩法特性前应检查能力标记。 |
| 灯光 | `ds5_set_lightbar`, `ds5_set_player_leds`, `ds5_set_mic_led` | 设置光条 RGB、玩家灯 mask、麦克风灯模式。 |
| 震动/触觉 | `ds5_set_rumble`, `ds5_set_haptic_pattern` | 经典 rumble 和一个按时长播放的 haptic 辅助函数。 |
| 自适应扳机 | `ds5_set_trigger_effect` | 支持关闭、恒定阻力、区间阻力、武器感、震动等模式。 |
| 原始 report | `ds5_send_raw_output_report` | 给协议实验使用的高级逃生口。 |
| 音频 | `ds5_audio_enumerate_endpoints`, `ds5_audio_play_pcm`, `ds5_audio_capture_start`, `ds5_audio_capture_stop` | 使用明确的 Windows 音频 endpoint id，不会修改系统默认音频设备。 |

C++ 封装声明在 `include/dualsense/dualsense.hpp`。

- `DualSense::Context` 持有 `ds5_context`。
- `DualSense::Controller` 持有 `ds5_device`。
- 被封装的 C 调用失败时抛出 `DualSense::Error`。
- `Controller::haptics()` 提供 `rumble` 和 `pattern`。
- `Controller::triggers()` 提供 `setEffect`、`setResistance` 和 `off`。
- 这层封装还提供光条、玩家灯、麦克风灯、raw output、能力查询、状态轮询和反馈复位辅助函数。

带有 `size` 和 `version` 的 C 结构体，在传给库之前都要初始化这两个字段：

```c
state.size = sizeof(state);
state.version = DS5_STRUCT_VERSION;
```

### 最小 C 示例

```c
#include <dualsense/dualsense.h>
#include <stdio.h>

int main(void) {
  ds5_context* context = NULL;
  if (ds5_init(&context) != DS5_OK) {
    puts(ds5_get_last_error());
    return 1;
  }

  uint32_t count = 0;
  ds5_result result = ds5_enumerate(context, NULL, 0, &count);
  if (result != DS5_OK && result != DS5_E_INSUFFICIENT_BUFFER) {
    puts(ds5_get_last_error());
    ds5_shutdown(context);
    return 1;
  }

  printf("DualSense controllers found: %u\n", count);
  ds5_shutdown(context);
  return 0;
}
```

仓库内对应示例是 `samples/c_api_sample.c`。

### 最小 C++ 示例

```cpp
#include <dualsense/dualsense.hpp>
#include <iostream>

int main() {
  try {
    DualSense::Context context;
    auto devices = context.enumerate();
    std::cout << "DualSense controllers found: " << devices.size() << "\n";
    if (!devices.empty()) {
      DualSense::Controller controller(context.native(), devices.front());
      controller.setLightbar(0, 64, 255);
      controller.haptics().rumble(32, 32);
      controller.triggers().setResistance(true, 16, 128);
      controller.resetFeedback();
    }
  } catch (const DualSense::Error& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
  return 0;
}
```

仓库内对应示例是 `samples/cpp_sample.cpp`。

### 功能矩阵

| 功能 | USB | Bluetooth | 说明 |
| --- | --- | --- | --- |
| 设备枚举 | 支持 | 有限支持 | Bluetooth 设备可能以降级能力被报告。 |
| 打开设备用于输入轮询 | 支持 | 有限支持 | Bluetooth 打开取决于 Windows 是否暴露可读 HID 路径。 |
| 打开设备用于 v1 输出控制 | 支持 | 不支持 | Bluetooth 输出 API 返回 `DS5_E_UNSUPPORTED_TRANSPORT`。 |
| 按钮、摇杆、扳机 | 支持 | 有限支持 | 已解析 USB 和 Bluetooth 短输入 report。 |
| 方向键 | 支持 | 有限支持 | 通过 `ds5_state.dpad` 返回。 |
| 触摸板 | 支持 | 仅扩展 report | 可用时返回两个 `ds5_touch_point`。 |
| 陀螺仪/加速度计 | 支持 | 仅扩展 report | 通过 `ds5_state.gyro_*` 和 `accel_*` 返回。 |
| 电量 | 支持 | 仅扩展 report | 通过 `battery_percent` 返回；v1 不建模充电状态。 |
| 光条 | 支持 | v1 Bluetooth 不支持 | `ds5_set_lightbar`。 |
| 玩家灯 | 支持 | v1 Bluetooth 不支持 | `ds5_set_player_leds`；mask 选择灯位。 |
| 麦克风灯 | 支持 | v1 Bluetooth 不支持 | `DS5_MIC_LED_OFF`、`ON` 或 `PULSE`。 |
| 经典 rumble | 支持 | v1 Bluetooth 不支持 | `ds5_set_rumble`。 |
| haptic pattern 辅助 | 支持 | v1 Bluetooth 不支持 | `ds5_set_haptic_pattern`。 |
| 自适应扳机 | 支持 | v1 Bluetooth 不支持 | `ds5_set_trigger_effect`。 |
| 原始输出 report | 支持 | v1 Bluetooth 不支持 | 仅建议高级用途使用。 |
| 扬声器播放 | 支持，通过 Windows 音频 endpoint | 不在 v1 范围内 | 使用 `ds5_audio_enumerate_endpoints` 返回的明确 endpoint id。 |
| 麦克风采集 | 支持，通过 Windows 音频 endpoint | 不在 v1 范围内 | 使用 `ds5_audio_capture_start`。 |
| 耳机孔路由 | 取决于硬件和 Windows endpoint | 不在 v1 范围内 | 库只枚举匹配 endpoint，不会切换 Windows 默认设备。 |

### 诊断和演示命令

以下命令从仓库根目录运行。如果你使用了其他构建目录或配置，请相应替换 `Debug` 和路径。

列出手柄和音频 endpoint：

```powershell
.\build-vs2026\Debug\dualsense_diag.exe
```

对第一个手柄运行短暂输出测试：

```powershell
.\build-vs2026\Debug\dualsense_diag.exe --test
```

打印已打开设备的 capabilities：

```powershell
.\build-vs2026\Debug\dualsense_diag.exe --capabilities
```

轮询输入 report：

```powershell
.\build-vs2026\Debug\dualsense_diag.exe --poll 180
```

在第一个匹配的播放 endpoint 上播放测试音：

```powershell
.\build-vs2026\Debug\dualsense_diag.exe --tone 1500
```

从第一个匹配的麦克风 endpoint 采集一秒：

```powershell
.\build-vs2026\Debug\dualsense_diag.exe --capture 1000
```

发送一个已知安全的 USB raw output reset report：

```powershell
.\build-vs2026\Debug\dualsense_diag.exe --raw-output-reset
```

运行飞船 demo：

```powershell
.\build-vs2026\Debug\dualsense_ship_demo.exe
```

飞船 demo 也支持 XInput 和键盘 fallback。具体控制和测试步骤见 `docs/ship-demo-test-steps.md`。

### 硬件注意事项

- USB 是完整功能集的预期连接方式。请使用能传数据的 USB 线，不要使用只能充电的线。
- 如果能枚举但输出特性失败，请确认手柄是 USB 连接，而不是 Bluetooth 连接。
- v1 对 Bluetooth 支持很保守：可能可以发现设备并轮询输入，但不会启用完整输出控制。
- 音频通过 Windows MMDevice/WASAPI endpoint 处理。手柄扬声器、麦克风和耳机孔在操作系统和设备暴露时会显示为 Windows 音频 endpoint。
- 本库不会修改 Windows 默认播放或录音设备。调用方需要明确选择 endpoint id。
- 如果扬声器或麦克风没有出现，请检查 Windows 声音设置、麦克风隐私权限、设备固件，以及线缆/USB 端口质量。
- 固件更新不属于本库范围。请使用 Sony PlayStation Accessories 维护固件。

### 错误处理清单

- 检查每个 `ds5_result`。
- 失败时记录 `ds5_get_last_error()`。
- 传入结构体前初始化 `size` 和 `version` 字段。
- 使用可选功能前检查 capability flags。
- 游戏或诊断程序退出前重置 rumble 和扳机效果。
- 关闭 device 后再关闭 context。
