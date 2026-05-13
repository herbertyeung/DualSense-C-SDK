# DualSense C SDK

Windows USB-first C SDK for Sony PlayStation 5 DualSense controllers.

This project provides a reusable C ABI, a small C++ RAII wrapper, diagnostic tools, samples, and a DirectX demo for exercising DualSense input, lightbar, player LEDs, microphone LED, rumble, adaptive triggers, controller audio endpoints, touchpad, and IMU data on Windows.

The SDK is currently a developer-preview release. The core USB paths are usable, but packaging, ABI policy, bindings, CI, and product-grade async I/O still need more hardening before a stable 1.0 release.

## English

### Scope

- Platform: Windows 10/11.
- Primary transport: USB.
- Reduced transport: Bluetooth input discovery/polling when Windows exposes a readable HID path.
- Library API: C ABI in `include/dualsense/dualsense.h`.
- C++ convenience layer: `include/dualsense/dualsense.hpp`.
- Diagnostics: `tools/dualsense_diag.cpp`.
- Integrated demo: `tools/dualsense_ship_demo.cpp`.

### Features

- Enumerate and open DualSense controllers through Windows HID.
- Poll buttons, sticks, triggers, D-pad, touchpad, battery, gyro, accelerometer, and raw reports.
- Control lightbar RGB, player LEDs, microphone LED, rumble, and adaptive triggers over USB.
- Enumerate DualSense speaker/microphone endpoints through Windows MMDevice/WASAPI.
- Play PCM audio to the controller speaker endpoint and capture microphone PCM data.
- Use `dualsense_diag` for device, capability, input, output, raw report, speaker, and microphone checks.
- Use the DirectX ship demo as an integrated controller-feedback sample.

### Build And Test

```powershell
cmake -S . -B build-vs2026 -G "Visual Studio 17 2022" -A x64
cmake --build build-vs2026 --config Debug
ctest --test-dir build-vs2026 -C Debug --output-on-failure
```

List devices and audio endpoints:

```powershell
.\build-vs2026\Debug\dualsense_diag.exe
```

Run the main output API test:

```powershell
.\build-vs2026\Debug\dualsense_diag.exe --test
```

### Use From Another CMake Project

Install the SDK, then point `CMAKE_PREFIX_PATH` at the install prefix:

```powershell
cmake --install build-vs2026 --config Debug --prefix C:\SDKs\DualSense
cmake -S examples\cmake-consumer -B build-consumer -DCMAKE_PREFIX_PATH=C:\SDKs\DualSense
cmake --build build-consumer --config Debug
```

In a downstream project:

```cmake
find_package(DualSense CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE DualSense::dualsense_static)
```

### Documentation

- `docs/user-guide.md`: build, API overview, feature matrix, diagnostics, and hardware notes.
- `docs/protocol-notes.md`: current USB/Bluetooth/HID/audio scope.
- `docs/ship-demo-test-steps.md`: DirectX demo verification steps.
- `docs/productization-plan.md`: current product-readiness assessment and development plan.
- `docs/packaging.md`: install layout and downstream CMake consumption.
- `docs/known-limitations.md`: current transport, audio, firmware, runtime, and testing limits.
- `docs/release-checklist.md`: build, package, consumer, and USB hardware release checks.
- `CHANGELOG.md`: public SDK version history.

### Productization Status

See `docs/productization-plan.md` for the detailed assessment. Short version: the code is already useful as a Windows USB DualSense SDK prototype, but it needs more API polish, packaging, test separation, async I/O, CI, and binding support before it should be called product-ready.

## 中文

### 项目定位

这是一个面向 Windows 的 USB 优先 DualSense C SDK，用于让普通 C/C++ 程序、游戏、工具或其它语言 binding 调用 PS5 DualSense 手柄能力。

当前项目处于 developer preview 阶段：USB 核心路径已经可用，已有 C ABI、C++ RAII 封装、诊断工具、示例和 DirectX 演示程序；但在稳定 1.0 发布前，还需要继续完善发布包、ABI 策略、语言绑定、CI、异步 I/O 和产品级文档。

### 当前范围

- 平台：Windows 10/11。
- 主要连接方式：USB。
- 降级支持：Bluetooth 在 Windows 暴露可读 HID 路径时可用于输入枚举/轮询，但 v1 不做完整输出控制。
- C API：`include/dualsense/dualsense.h`。
- C++ 封装：`include/dualsense/dualsense.hpp`。
- 诊断工具：`tools/dualsense_diag.cpp`。
- 集成演示：`tools/dualsense_ship_demo.cpp`。

### 已支持能力

- 通过 Windows HID 枚举和打开 DualSense 手柄。
- 读取按钮、摇杆、扳机、方向键、触摸板、电量、陀螺仪、加速度计和 raw report。
- USB 下控制光条 RGB、玩家灯、麦克风灯、rumble 和自适应扳机。
- 通过 Windows MMDevice/WASAPI 枚举 DualSense 扬声器和麦克风 endpoint。
- 向手柄扬声器播放 PCM 音频，并从麦克风采集 PCM 数据。
- 使用 `dualsense_diag` 检查设备、capabilities、输入、输出、raw report、扬声器和麦克风。
- 使用 DirectX 飞船 demo 作为完整的手柄输入和反馈集成示例。

### 构建和测试

```powershell
cmake -S . -B build-vs2026 -G "Visual Studio 17 2022" -A x64
cmake --build build-vs2026 --config Debug
ctest --test-dir build-vs2026 -C Debug --output-on-failure
```

列出手柄和音频 endpoint：

```powershell
.\build-vs2026\Debug\dualsense_diag.exe
```

运行主要输出 API 测试：

```powershell
.\build-vs2026\Debug\dualsense_diag.exe --test
```

### 在其它 CMake 项目中使用

先安装 SDK，再让下游项目通过 `CMAKE_PREFIX_PATH` 找到安装目录：

```powershell
cmake --install build-vs2026 --config Debug --prefix C:\SDKs\DualSense
cmake -S examples\cmake-consumer -B build-consumer -DCMAKE_PREFIX_PATH=C:\SDKs\DualSense
cmake --build build-consumer --config Debug
```

下游项目中：

```cmake
find_package(DualSense CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE DualSense::dualsense_static)
```

### 文档

- `docs/user-guide.md`：构建、API 概览、功能矩阵、诊断命令和硬件说明。
- `docs/protocol-notes.md`：当前 USB/Bluetooth/HID/audio 范围。
- `docs/ship-demo-test-steps.md`：DirectX demo 验证步骤。
- `docs/productization-plan.md`：当前产品化评估和开发完善计划。
- `docs/packaging.md`：安装目录结构和下游 CMake 集成方式。
- `docs/known-limitations.md`：当前传输、音频、固件、运行时和测试限制。
- `docs/release-checklist.md`：构建、打包、consumer 和 USB 硬件发布检查。
- `CHANGELOG.md`：公开 SDK 版本历史。
