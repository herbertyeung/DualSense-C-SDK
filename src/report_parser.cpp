#include "report_parser.h"

#include <algorithm>
#include <cstring>

namespace {

constexpr uint8_t kShortInputReportId = 0x01u;
constexpr uint8_t kBluetoothExtendedInputReportId = 0x31u;
constexpr size_t kBluetoothShortReportSize = 10u;
constexpr size_t kExtendedReportMinSize = 17u;
constexpr size_t kPayloadMinSize = 16u;
constexpr size_t kReportIdSize = 1u;

constexpr size_t kLeftStickXOffset = 0u;
constexpr size_t kLeftStickYOffset = 1u;
constexpr size_t kRightStickXOffset = 2u;
constexpr size_t kRightStickYOffset = 3u;
constexpr size_t kLeftTriggerOffset = 4u;
constexpr size_t kRightTriggerOffset = 5u;
constexpr size_t kButton0Offset = 7u;
constexpr size_t kButton1Offset = 8u;
constexpr size_t kButton2Offset = 9u;
constexpr size_t kBatteryOffset = 10u;
constexpr size_t kImuOffset = 15u;
constexpr size_t kImuBytes = 12u;
constexpr size_t kFirstTouchOffset = 29u;
constexpr size_t kSecondTouchOffset = 33u;
constexpr size_t kTouchBytesRequired = 37u;

constexpr size_t kBluetoothShortLeftStickXOffset = 1u;
constexpr size_t kBluetoothShortLeftStickYOffset = 2u;
constexpr size_t kBluetoothShortRightStickXOffset = 3u;
constexpr size_t kBluetoothShortRightStickYOffset = 4u;
constexpr size_t kBluetoothShortButton0Offset = 5u;
constexpr size_t kBluetoothShortButton1Offset = 6u;
constexpr size_t kBluetoothShortButton2Offset = 7u;
constexpr size_t kBluetoothShortLeftTriggerOffset = 8u;
constexpr size_t kBluetoothShortRightTriggerOffset = 9u;

constexpr size_t kTouchStatusAndIdOffset = 0u;
constexpr size_t kTouchXLowOffset = 1u;
constexpr size_t kTouchXYHighOffset = 2u;
constexpr size_t kTouchYHighOffset = 3u;

int16_t read_i16_le(const uint8_t* bytes) {
  return static_cast<int16_t>(static_cast<uint16_t>(bytes[0]) | (static_cast<uint16_t>(bytes[1]) << 8));
}

void parse_touch_point(const uint8_t* bytes, ds5_touch_point* point) {
  point->active = (bytes[kTouchStatusAndIdOffset] & 0x80u) == 0u ? 1u : 0u;
  point->id = bytes[kTouchStatusAndIdOffset] & 0x7fu;
  point->x = static_cast<uint16_t>(bytes[kTouchXLowOffset] | ((bytes[kTouchXYHighOffset] & 0x0fu) << 8));
  point->y = static_cast<uint16_t>(((bytes[kTouchXYHighOffset] >> 4) & 0x0fu) | (bytes[kTouchYHighOffset] << 4));
}

void parse_standard_buttons(uint8_t b0, uint8_t b1, uint8_t b2, ds5_state* state, bool has_mute) {
  const uint8_t dpad = b0 & 0x0fu;
  state->dpad = dpad <= DS5_DPAD_NONE ? static_cast<ds5_dpad>(dpad) : DS5_DPAD_NONE;

  if (b0 & 0x10u) state->buttons |= DS5_BUTTON_SQUARE;
  if (b0 & 0x20u) state->buttons |= DS5_BUTTON_CROSS;
  if (b0 & 0x40u) state->buttons |= DS5_BUTTON_CIRCLE;
  if (b0 & 0x80u) state->buttons |= DS5_BUTTON_TRIANGLE;
  if (b1 & 0x01u) state->buttons |= DS5_BUTTON_L1;
  if (b1 & 0x02u) state->buttons |= DS5_BUTTON_R1;
  if (b1 & 0x04u) state->buttons |= DS5_BUTTON_L2;
  if (b1 & 0x08u) state->buttons |= DS5_BUTTON_R2;
  if (b1 & 0x10u) state->buttons |= DS5_BUTTON_CREATE;
  if (b1 & 0x20u) state->buttons |= DS5_BUTTON_OPTIONS;
  if (b1 & 0x40u) state->buttons |= DS5_BUTTON_L3;
  if (b1 & 0x80u) state->buttons |= DS5_BUTTON_R3;
  if (b2 & 0x01u) state->buttons |= DS5_BUTTON_PS;
  if (b2 & 0x02u) state->buttons |= DS5_BUTTON_TOUCHPAD;
  if (has_mute && (b2 & 0x04u)) state->buttons |= DS5_BUTTON_MUTE;
}

void clear_state(ds5_state* state, ds5_transport transport) {
  std::memset(state, 0, sizeof(*state));
  state->size = sizeof(*state);
  state->version = DS5_STRUCT_VERSION;
  state->transport = transport;
  state->dpad = DS5_DPAD_NONE;
}

}  // namespace

ds5_capabilities ds5_internal_capabilities_for_transport(ds5_transport transport) {
  ds5_capabilities caps{};
  ds5_capabilities_init(&caps);
  caps.transport = transport;
  caps.flags = DS5_CAP_INPUT | DS5_CAP_TOUCHPAD | DS5_CAP_IMU | DS5_CAP_RAW_REPORTS;

  if (transport == DS5_TRANSPORT_USB) {
    caps.flags |= DS5_CAP_LIGHTBAR | DS5_CAP_PLAYER_LEDS | DS5_CAP_MIC_LED | DS5_CAP_CLASSIC_RUMBLE |
                  DS5_CAP_HAPTICS | DS5_CAP_ADAPTIVE_TRIGGERS | DS5_CAP_AUDIO_SPEAKER |
                  DS5_CAP_AUDIO_MICROPHONE | DS5_CAP_HEADSET_JACK;
  }

  return caps;
}

ds5_result ds5_internal_parse_input_report(const uint8_t* bytes, size_t size, ds5_transport transport, ds5_state* state) {
  if (!bytes || !state) {
    return DS5_E_INVALID_ARGUMENT;
  }

  clear_state(state, transport);

  if (transport == DS5_TRANSPORT_BLUETOOTH && bytes[0] == kShortInputReportId) {
    if (size < kBluetoothShortReportSize) {
      return DS5_E_INVALID_ARGUMENT;
    }
    // Windows may expose Bluetooth input as a short report with no IMU,
    // touchpad, battery, or mute fields. Keep it as input-only in v1.
    state->left_stick_x = bytes[kBluetoothShortLeftStickXOffset];
    state->left_stick_y = bytes[kBluetoothShortLeftStickYOffset];
    state->right_stick_x = bytes[kBluetoothShortRightStickXOffset];
    state->right_stick_y = bytes[kBluetoothShortRightStickYOffset];
    parse_standard_buttons(bytes[kBluetoothShortButton0Offset], bytes[kBluetoothShortButton1Offset],
                           bytes[kBluetoothShortButton2Offset], state, false);
    state->left_trigger = bytes[kBluetoothShortLeftTriggerOffset];
    state->right_trigger = bytes[kBluetoothShortRightTriggerOffset];
    state->raw_report_size = static_cast<uint32_t>(std::min<size_t>(size, DS5_RAW_REPORT_MAX));
    std::memcpy(state->raw_report, bytes, state->raw_report_size);
    return DS5_OK;
  }

  if (size < kExtendedReportMinSize) {
    return DS5_E_INVALID_ARGUMENT;
  }

  size_t offset = 0u;
  if (bytes[0] == kShortInputReportId || bytes[0] == kBluetoothExtendedInputReportId) {
    offset = kReportIdSize;
  }
  if (size < offset + kPayloadMinSize) {
    return DS5_E_INVALID_ARGUMENT;
  }

  state->left_stick_x = bytes[offset + kLeftStickXOffset];
  state->left_stick_y = bytes[offset + kLeftStickYOffset];
  state->right_stick_x = bytes[offset + kRightStickXOffset];
  state->right_stick_y = bytes[offset + kRightStickYOffset];
  state->left_trigger = bytes[offset + kLeftTriggerOffset];
  state->right_trigger = bytes[offset + kRightTriggerOffset];

  uint8_t b2 = 0u;
  if (size > offset + kButton2Offset) {
    b2 = bytes[offset + kButton2Offset];
  }
  parse_standard_buttons(bytes[offset + kButton0Offset], bytes[offset + kButton1Offset], b2, state, true);

  if (size > offset + kBatteryOffset) {
    state->battery_percent = bytes[offset + kBatteryOffset];
  }

  // Extended reports store four reserved/status bytes after the button block.
  // IMU samples start at payload offset 15 and are packed as six little-endian i16 values.
  const size_t imu_offset = offset + kImuOffset;
  if (size >= imu_offset + kImuBytes) {
    state->gyro_x = read_i16_le(bytes + imu_offset + 0u);
    state->gyro_y = read_i16_le(bytes + imu_offset + 2u);
    state->gyro_z = read_i16_le(bytes + imu_offset + 4u);
    state->accel_x = read_i16_le(bytes + imu_offset + 6u);
    state->accel_y = read_i16_le(bytes + imu_offset + 8u);
    state->accel_z = read_i16_le(bytes + imu_offset + 10u);
  }

  if (size >= offset + kTouchBytesRequired) {
    parse_touch_point(bytes + offset + kFirstTouchOffset, &state->touch[0]);
    parse_touch_point(bytes + offset + kSecondTouchOffset, &state->touch[1]);
  }

  state->raw_report_size = static_cast<uint32_t>(std::min<size_t>(size, DS5_RAW_REPORT_MAX));
  std::memcpy(state->raw_report, bytes, state->raw_report_size);
  return DS5_OK;
}
