#include "report_parser.h"

#include <algorithm>
#include <cstring>

namespace {

int16_t read_i16_le(const uint8_t* bytes) {
  return static_cast<int16_t>(static_cast<uint16_t>(bytes[0]) | (static_cast<uint16_t>(bytes[1]) << 8));
}

void parse_touch_point(const uint8_t* bytes, ds5_touch_point* point) {
  point->active = (bytes[0] & 0x80u) == 0u ? 1u : 0u;
  point->id = bytes[0] & 0x7fu;
  point->x = static_cast<uint16_t>(bytes[1] | ((bytes[2] & 0x0fu) << 8));
  point->y = static_cast<uint16_t>(((bytes[2] >> 4) & 0x0fu) | (bytes[3] << 4));
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
  caps.size = sizeof(caps);
  caps.version = DS5_STRUCT_VERSION;
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

  if (transport == DS5_TRANSPORT_BLUETOOTH && bytes[0] == 0x01u) {
    if (size < 10u) {
      return DS5_E_INVALID_ARGUMENT;
    }
    state->left_stick_x = bytes[1];
    state->left_stick_y = bytes[2];
    state->right_stick_x = bytes[3];
    state->right_stick_y = bytes[4];
    parse_standard_buttons(bytes[5], bytes[6], bytes[7], state, false);
    state->left_trigger = bytes[8];
    state->right_trigger = bytes[9];
    state->raw_report_size = static_cast<uint32_t>(std::min<size_t>(size, DS5_RAW_REPORT_MAX));
    std::memcpy(state->raw_report, bytes, state->raw_report_size);
    return DS5_OK;
  }

  if (size < 17u) {
    return DS5_E_INVALID_ARGUMENT;
  }

  size_t offset = 0u;
  if (bytes[0] == 0x01u || bytes[0] == 0x31u) {
    offset = 1u;
  }
  if (size < offset + 16u) {
    return DS5_E_INVALID_ARGUMENT;
  }

  state->left_stick_x = bytes[offset + 0u];
  state->left_stick_y = bytes[offset + 1u];
  state->right_stick_x = bytes[offset + 2u];
  state->right_stick_y = bytes[offset + 3u];
  state->left_trigger = bytes[offset + 4u];
  state->right_trigger = bytes[offset + 5u];

  uint8_t b2 = 0u;
  if (size > offset + 9u) {
    b2 = bytes[offset + 9u];
  }
  parse_standard_buttons(bytes[offset + 7u], bytes[offset + 8u], b2, state, true);

  if (size > offset + 10u) {
    state->battery_percent = bytes[offset + 10u];
  }

  // USB report 0x01 stores four reserved/status bytes after the button block.
  // IMU samples start at report byte 16, i.e. payload offset + 15.
  const size_t imu_offset = offset + 15u;
  if (size >= imu_offset + 12u) {
    state->gyro_x = read_i16_le(bytes + imu_offset + 0u);
    state->gyro_y = read_i16_le(bytes + imu_offset + 2u);
    state->gyro_z = read_i16_le(bytes + imu_offset + 4u);
    state->accel_x = read_i16_le(bytes + imu_offset + 6u);
    state->accel_y = read_i16_le(bytes + imu_offset + 8u);
    state->accel_z = read_i16_le(bytes + imu_offset + 10u);
  }

  if (size >= offset + 37u) {
    parse_touch_point(bytes + offset + 29u, &state->touch[0]);
    parse_touch_point(bytes + offset + 33u, &state->touch[1]);
  }

  state->raw_report_size = static_cast<uint32_t>(std::min<size_t>(size, DS5_RAW_REPORT_MAX));
  std::memcpy(state->raw_report, bytes, state->raw_report_size);
  return DS5_OK;
}
