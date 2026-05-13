# DualSense Protocol Notes

## English

This SDK targets Windows 10/11 with a USB-connected DualSense controller for full v1 support. For user-facing setup, examples, and commands, see `docs/user-guide.md`.

### Transport Policy

- USB is the full-capability transport for HID output reports, advanced haptics, adaptive triggers, speaker, microphone, and headset jack routing.
- Bluetooth can be opened for input polling when Windows exposes a readable HID path, but v1 does not implement Bluetooth output reports.
- Firmware updates are delegated to Sony PlayStation Accessories.

### HID Scope

- Input reports are parsed into a stable `ds5_state` structure and retain raw bytes for debugging.
- Opened HID handles use overlapped I/O. `ds5_poll_state` waits indefinitely, `ds5_poll_state_timeout` waits for the caller-provided bound, and `ds5_try_poll_state` returns `DS5_E_TIMEOUT` when no report is ready immediately.
- Output reports are built through `ds5_internal_build_usb_output_report`.
- `ds5_reset_feedback` clears SDK-managed rumble, adaptive triggers, and mic LED in one output report while preserving lightbar and player LEDs.
- `ds5_send_raw_output_report` is available as an explicit advanced escape hatch for protocol experiments.
- Callers should prefer the typed APIs for lightbar, player LEDs, mic LED, rumble, haptics, and adaptive triggers before falling back to raw output reports.
- `ds5_send_raw_output_report` is USB-only in v1; Bluetooth output needs a different report envelope and checksum.

### Audio Scope

- Audio endpoints are discovered through MMDevice/WASAPI by matching endpoint names commonly exposed for DualSense devices.
- Playback and capture helpers operate on explicit endpoint ids and do not change Windows default devices.
- The speaker, microphone, and headset jack depend on what Windows exposes for the connected controller and cable/port combination.

## 中文

本 SDK 的 v1 完整支持目标是 Windows 10/11，并通过 USB 连接 DualSense 手柄。面向用户的安装、示例和命令请看 `docs/user-guide.md`。

### 传输策略

- USB 是完整能力传输方式，覆盖 HID 输出 report、高级触觉、自适应扳机、扬声器、麦克风和耳机孔路由。
- 当 Windows 暴露可读 HID 路径时，Bluetooth 设备可以打开并轮询输入，但 v1 不实现 Bluetooth 输出 report。
- 固件更新交给 Sony PlayStation Accessories 处理。

### HID 范围

- 输入 report 会被解析成稳定的 `ds5_state` 结构，同时保留原始字节用于调试。
- 打开的 HID handle 使用 overlapped I/O。`ds5_poll_state` 会无限等待，`ds5_poll_state_timeout` 按调用方给定时限等待，`ds5_try_poll_state` 在没有立即可用 report 时返回 `DS5_E_TIMEOUT`。
- 输出 report 通过 `ds5_internal_build_usb_output_report` 构造。
- `ds5_reset_feedback` 会用一个输出 report 清理 SDK 管理的 rumble、自适应扳机和麦克风灯，同时保留 lightbar 和玩家灯。
- `ds5_send_raw_output_report` 是明确提供给协议实验使用的高级逃生口。
- 调用方应优先使用 lightbar、玩家灯、麦克风灯、rumble、haptics 和自适应扳机这些类型化 API；只有协议实验才需要退回 raw output report。
- `ds5_send_raw_output_report` 在 v1 中仅支持 USB；Bluetooth 输出需要不同的 report 封装和校验。

### 音频范围

- 音频 endpoint 通过 MMDevice/WASAPI 发现，并匹配 DualSense 设备常见的 endpoint 名称。
- 播放和采集辅助函数使用明确的 endpoint id，不会修改 Windows 默认设备。
- 扬声器、麦克风和耳机孔是否可用，取决于 Windows 对当前手柄、线缆和端口组合暴露了哪些 endpoint。
