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
  report.size = 48u;
  std::memset(report.bytes, 0, sizeof(report.bytes));

  report.bytes[0] = 0x02u;
  report.bytes[1] = kValidFlag0CompatibleVibration | kValidFlag0HapticsSelect |
                    kValidFlag0RightTriggerMotor | kValidFlag0LeftTriggerMotor;
  report.bytes[2] = kValidFlag1MicLed | kValidFlag1Lightbar | kValidFlag1PlayerLeds;

  if (!state) {
    return report;
  }

  report.bytes[3] = state->right_rumble;
  report.bytes[4] = state->left_rumble;
  report.bytes[9] = static_cast<uint8_t>(state->mic_led);
  write_trigger(&report.bytes[11], state->right_trigger);
  write_trigger(&report.bytes[22], state->left_trigger);
  report.bytes[44] = state->player_leds & 0x1fu;
  report.bytes[45] = state->lightbar_r;
  report.bytes[46] = state->lightbar_g;
  report.bytes[47] = state->lightbar_b;
  return report;
}
