#include "output_report.h"

#include <cstring>

namespace {

constexpr uint8_t kValidFlag0CompatibleVibration = 1u << 0u;
constexpr uint8_t kValidFlag0HapticsSelect = 1u << 1u;
constexpr uint8_t kValidFlag0RightTriggerMotor = 1u << 2u;
constexpr uint8_t kValidFlag0LeftTriggerMotor = 1u << 3u;
constexpr uint8_t kValidFlag1MicLed = 1u << 0u;
constexpr uint8_t kValidFlag1Lightbar = 1u << 2u;
constexpr uint8_t kValidFlag1PlayerLeds = 1u << 4u;

constexpr uint32_t kUsbOutputReportSize = 48u;
constexpr uint8_t kUsbOutputReportId = 0x02u;
constexpr size_t kReportIdOffset = 0u;
constexpr size_t kValidFlag0Offset = 1u;
constexpr size_t kValidFlag1Offset = 2u;
constexpr size_t kRightRumbleOffset = 3u;
constexpr size_t kLeftRumbleOffset = 4u;
constexpr size_t kMicLedOffset = 9u;
constexpr size_t kRightTriggerOffset = 11u;
constexpr size_t kLeftTriggerOffset = 22u;
constexpr size_t kPlayerLedsOffset = 44u;
constexpr size_t kLightbarRedOffset = 45u;
constexpr size_t kLightbarGreenOffset = 46u;
constexpr size_t kLightbarBlueOffset = 47u;

// Trigger blocks are ten bytes in the USB output report. The SDK's public
// trigger modes use the first five bytes; the remaining bytes stay zero.
void write_trigger(uint8_t* target, const ds5_trigger_effect& effect) {
  target[0] = static_cast<uint8_t>(effect.mode);
  target[1] = effect.start_position;
  target[2] = effect.force;
  target[3] = effect.end_position;
  target[4] = effect.frequency;
  target[5] = 0;
  target[6] = 0;
  target[7] = 0;
  target[8] = 0;
  target[9] = 0;
}

}  // namespace

ds5_internal_output_report ds5_internal_build_usb_output_report(const ds5_output_state* state) {
  ds5_internal_output_report report{};
  report.size = kUsbOutputReportSize;
  std::memset(report.bytes, 0, sizeof(report.bytes));

  report.bytes[kReportIdOffset] = kUsbOutputReportId;
  report.bytes[kValidFlag0Offset] = kValidFlag0CompatibleVibration | kValidFlag0HapticsSelect |
                                    kValidFlag0RightTriggerMotor | kValidFlag0LeftTriggerMotor;
  report.bytes[kValidFlag1Offset] = kValidFlag1MicLed | kValidFlag1Lightbar | kValidFlag1PlayerLeds;

  if (!state) {
    return report;
  }

  report.bytes[kRightRumbleOffset] = state->right_rumble;
  report.bytes[kLeftRumbleOffset] = state->left_rumble;
  report.bytes[kMicLedOffset] = static_cast<uint8_t>(state->mic_led);
  write_trigger(&report.bytes[kRightTriggerOffset], state->right_trigger);
  write_trigger(&report.bytes[kLeftTriggerOffset], state->left_trigger);
  report.bytes[kPlayerLedsOffset] = state->player_leds & 0x1fu;
  report.bytes[kLightbarRedOffset] = state->lightbar_r;
  report.bytes[kLightbarGreenOffset] = state->lightbar_g;
  report.bytes[kLightbarBlueOffset] = state->lightbar_b;
  return report;
}
