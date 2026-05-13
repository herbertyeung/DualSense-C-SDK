#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <dualsense/dualsense.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "core.h"
#include "output_report.h"
#include "report_parser.h"
#include "../tools/ship_controls.h"
#include "../tools/ship_feedback.h"
#include "../tools/ship_systems.h"
#include "../tools/wav_pcm.h"

namespace {

class TestFailure : public std::runtime_error {
 public:
  explicit TestFailure(const std::string& message) : std::runtime_error(message) {}
};

const char* g_current_test = "<none>";

void expect(bool condition, const char* expression, const char* file, int line) {
  if (condition) {
    return;
  }

  std::string message = "FAILED ";
  message += g_current_test;
  message += ": ";
  message += expression;
  message += " at ";
  message += file;
  message += ":";
  message += std::to_string(line);
  throw TestFailure(message);
}

void run_test(const char* name, void (*test)()) {
  g_current_test = name;
  test();
  std::cout << "PASS " << name << "\n";
}

}  // namespace

#define assert(expression) expect((expression), #expression, __FILE__, __LINE__)

static void test_abi_structs_are_versioned() {
  ds5_device_info info{};
  ds5_device_info_init(&info);
  assert(info.size == sizeof(ds5_device_info));
  assert(info.version == 1u);
  assert(info.capabilities.size == sizeof(ds5_capabilities));
  assert(info.capabilities.version == 1u);

  ds5_state state{};
  ds5_state_init(&state);
  assert(state.size == sizeof(ds5_state));
  assert(state.version == 1u);
  assert(state.dpad == DS5_DPAD_NONE);
  assert(state.raw_report_size == 0u);
}

static void test_public_helpers_report_version_and_results() {
  uint32_t major = 99;
  uint32_t minor = 99;
  uint32_t patch = 99;
  ds5_get_version(&major, &minor, &patch);
  assert(major == DS5_VERSION_MAJOR);
  assert(minor == DS5_VERSION_MINOR);
  assert(patch == DS5_VERSION_PATCH);
  assert(std::strcmp(ds5_get_version_string(), DS5_VERSION_STRING) == 0);
  assert(std::strcmp(ds5_result_to_string(DS5_OK), "DS5_OK") == 0);
  assert(std::strcmp(ds5_result_to_string(DS5_E_TIMEOUT), "DS5_E_TIMEOUT") == 0);
  assert(std::strcmp(ds5_result_to_string(static_cast<ds5_result>(-999)), "DS5_E_UNKNOWN") == 0);
}

static void test_public_trigger_builders() {
  ds5_trigger_effect effect{};
  ds5_trigger_effect_constant_resistance(&effect, 12, 180);
  assert(effect.mode == DS5_TRIGGER_EFFECT_CONSTANT_RESISTANCE);
  assert(effect.start_position == 12);
  assert(effect.force == 180);

  ds5_trigger_effect_section_resistance(&effect, 4, 18, 160);
  assert(effect.mode == DS5_TRIGGER_EFFECT_SECTION_RESISTANCE);
  assert(effect.start_position == 4);
  assert(effect.end_position == 18);
  assert(effect.force == 160);

  ds5_trigger_effect_weapon(&effect, 3, 9, 220);
  assert(effect.mode == DS5_TRIGGER_EFFECT_WEAPON);
  assert(effect.end_position == 9);

  ds5_trigger_effect_vibration(&effect, 2, 20, 200, 33);
  assert(effect.mode == DS5_TRIGGER_EFFECT_VIBRATION);
  assert(effect.frequency == 33);

  ds5_trigger_effect_off(&effect);
  assert(effect.mode == DS5_TRIGGER_EFFECT_OFF);
  assert(effect.force == 0);
}

static void test_public_struct_version_is_enforced() {
  ds5_context context{};
  ds5_device device{};
  device.context = &context;
  device.info.capabilities = ds5_internal_capabilities_for_transport(DS5_TRANSPORT_USB);

  ds5_capabilities caps{};
  ds5_capabilities_init(&caps);
  caps.version = DS5_STRUCT_VERSION + 1u;
  assert(ds5_get_capabilities(&device, &caps) == DS5_E_INVALID_ARGUMENT);
}

static void test_poll_timeout_validates_arguments() {
  ds5_state state{};
  ds5_state_init(&state);
  assert(ds5_poll_state_timeout(nullptr, 0, &state) == DS5_E_INVALID_ARGUMENT);

  ds5_context context{};
  ds5_device device{};
  device.context = &context;
  state.version = DS5_STRUCT_VERSION + 1u;
  assert(ds5_try_poll_state(&device, &state) == DS5_E_INVALID_ARGUMENT);
}

static void test_reset_feedback_clears_cached_output_before_transport_check() {
  ds5_context context{};
  ds5_device device{};
  device.context = &context;
  device.info.transport = DS5_TRANSPORT_BLUETOOTH;
  device.output.left_rumble = 20;
  device.output.right_rumble = 30;
  device.output.mic_led = DS5_MIC_LED_ON;
  ds5_trigger_effect_constant_resistance(&device.output.left_trigger, 12, 100);
  ds5_trigger_effect_vibration(&device.output.right_trigger, 4, 20, 180, 30);

  assert(ds5_reset_feedback(&device) == DS5_E_UNSUPPORTED_TRANSPORT);
  assert(device.output.left_rumble == 0);
  assert(device.output.right_rumble == 0);
  assert(device.output.mic_led == DS5_MIC_LED_OFF);
  assert(device.output.left_trigger.mode == DS5_TRIGGER_EFFECT_OFF);
  assert(device.output.right_trigger.mode == DS5_TRIGGER_EFFECT_OFF);
}

static void test_reset_feedback_rejects_null_device() {
  assert(ds5_reset_feedback(nullptr) == DS5_E_INVALID_ARGUMENT);
}

static void test_reset_feedback_output_encoding_is_clear() {
  ds5_output_state output{};
  output.left_rumble = 99;
  output.right_rumble = 88;
  output.mic_led = DS5_MIC_LED_ON;
  ds5_trigger_effect_constant_resistance(&output.left_trigger, 12, 100);
  ds5_trigger_effect_vibration(&output.right_trigger, 4, 20, 180, 30);

  output.left_rumble = 0;
  output.right_rumble = 0;
  output.mic_led = DS5_MIC_LED_OFF;
  ds5_trigger_effect_off(&output.left_trigger);
  ds5_trigger_effect_off(&output.right_trigger);

  ds5_internal_output_report report = ds5_internal_build_usb_output_report(&output);
  assert(report.bytes[3] == 0);
  assert(report.bytes[4] == 0);
  assert(report.bytes[9] == DS5_MIC_LED_OFF);
  assert(report.bytes[11] == DS5_TRIGGER_EFFECT_OFF);
  assert(report.bytes[22] == DS5_TRIGGER_EFFECT_OFF);
}

static void test_capabilities_for_usb_are_full_featured() {
  ds5_capabilities caps = ds5_internal_capabilities_for_transport(DS5_TRANSPORT_USB);
  assert((caps.flags & DS5_CAP_HAPTICS) != 0u);
  assert((caps.flags & DS5_CAP_ADAPTIVE_TRIGGERS) != 0u);
  assert((caps.flags & DS5_CAP_AUDIO_SPEAKER) != 0u);
  assert((caps.flags & DS5_CAP_AUDIO_MICROPHONE) != 0u);
}

static void test_capabilities_for_bluetooth_are_reduced() {
  ds5_capabilities caps = ds5_internal_capabilities_for_transport(DS5_TRANSPORT_BLUETOOTH);
  assert((caps.flags & DS5_CAP_HAPTICS) == 0u);
  assert((caps.flags & DS5_CAP_ADAPTIVE_TRIGGERS) == 0u);
  assert((caps.flags & DS5_CAP_LIGHTBAR) == 0u);
  assert((caps.flags & DS5_CAP_INPUT) != 0u);
}

static void test_usb_input_report_parser() {
  unsigned char report[64]{};
  report[0] = 0x01;
  report[1] = 128;
  report[2] = 129;
  report[3] = 130;
  report[4] = 131;
  report[5] = 25;
  report[6] = 230;
  report[8] = 0x30;
  report[9] = 0x24;
  report[11] = 80;
  report[16] = 0x01;
  report[17] = 0x02;
  report[18] = 0x03;
  report[19] = 0x04;
  report[20] = 0x05;
  report[21] = 0x06;
  report[22] = 0x07;
  report[23] = 0x08;
  report[24] = 0x09;
  report[25] = 0x0a;
  report[26] = 0x0b;
  report[27] = 0x0c;

  ds5_state state{};
  ds5_result result = ds5_internal_parse_input_report(report, sizeof(report), DS5_TRANSPORT_USB, &state);
  assert(result == DS5_OK);
  assert(state.left_stick_x == 128);
  assert(state.right_stick_y == 131);
  assert(state.left_trigger == 25);
  assert(state.right_trigger == 230);
  assert((state.buttons & DS5_BUTTON_CROSS) != 0u);
  assert((state.buttons & DS5_BUTTON_SQUARE) != 0u);
  assert(state.battery_percent == 80);
  assert(state.gyro_x == 0x0201);
  assert(state.gyro_y == 0x0403);
  assert(state.gyro_z == 0x0605);
  assert(state.accel_x == 0x0807);
  assert(state.accel_y == 0x0a09);
  assert(state.accel_z == 0x0c0b);
  assert(state.raw_report_size == sizeof(report));
}

static void test_bluetooth_short_input_report_parser() {
  unsigned char report[10]{};
  report[0] = 0x01;
  report[1] = 127;
  report[2] = 128;
  report[3] = 129;
  report[4] = 130;
  report[5] = 0x24;
  report[6] = 0x82;
  report[7] = 0x03;
  report[8] = 44;
  report[9] = 210;

  ds5_state state{};
  ds5_result result = ds5_internal_parse_input_report(report, sizeof(report), DS5_TRANSPORT_BLUETOOTH, &state);
  assert(result == DS5_OK);
  assert(state.left_stick_x == 127);
  assert(state.left_stick_y == 128);
  assert(state.right_stick_x == 129);
  assert(state.right_stick_y == 130);
  assert(state.left_trigger == 44);
  assert(state.right_trigger == 210);
  assert(state.dpad == DS5_DPAD_DOWN);
  assert((state.buttons & DS5_BUTTON_CROSS) != 0u);
  assert((state.buttons & DS5_BUTTON_R1) != 0u);
  assert((state.buttons & DS5_BUTTON_R3) != 0u);
  assert((state.buttons & DS5_BUTTON_PS) != 0u);
  assert((state.buttons & DS5_BUTTON_TOUCHPAD) != 0u);
  assert(state.transport == DS5_TRANSPORT_BLUETOOTH);
  assert(state.raw_report_size == sizeof(report));
}

static void test_bluetooth_output_reports_are_rejected_before_io() {
  ds5_context context{};
  ds5_device device{};
  device.context = &context;
  device.info.transport = DS5_TRANSPORT_BLUETOOTH;
  device.handle = INVALID_HANDLE_VALUE;

  ds5_result result = ds5_set_rumble(&device, 10, 20);
  assert(result == DS5_E_UNSUPPORTED_TRANSPORT);
}

static void test_output_report_builder_light_rumble_trigger() {
  ds5_output_state output{};
  output.lightbar_r = 10;
  output.lightbar_g = 20;
  output.lightbar_b = 30;
  output.player_leds = 0x15;
  output.mic_led = DS5_MIC_LED_ON;
  output.left_rumble = 64;
  output.right_rumble = 192;
  output.left_trigger.mode = DS5_TRIGGER_EFFECT_CONSTANT_RESISTANCE;
  output.left_trigger.start_position = 12;
  output.left_trigger.force = 200;

  ds5_internal_output_report report = ds5_internal_build_usb_output_report(&output);
  assert(report.size == 48u);
  assert(report.bytes[0] == 0x02);
  assert(report.bytes[1] == 0x0f);
  assert(report.bytes[2] == 0x15);
  assert(report.bytes[3] == 192);
  assert(report.bytes[4] == 64);
  assert(report.bytes[9] == DS5_MIC_LED_ON);
  assert(report.bytes[22] == DS5_TRIGGER_EFFECT_CONSTANT_RESISTANCE);
  assert(report.bytes[23] == 12);
  assert(report.bytes[24] == 200);
  assert(report.bytes[44] == 0x15);
  assert(report.bytes[45] == 10);
  assert(report.bytes[46] == 20);
  assert(report.bytes[47] == 30);
}

static void test_output_report_builder_encodes_all_public_trigger_modes() {
  ds5_output_state output{};
  output.right_trigger.mode = DS5_TRIGGER_EFFECT_SECTION_RESISTANCE;
  output.right_trigger.start_position = 8;
  output.right_trigger.end_position = 22;
  output.right_trigger.force = 140;
  output.left_trigger.mode = DS5_TRIGGER_EFFECT_WEAPON;
  output.left_trigger.start_position = 3;
  output.left_trigger.end_position = 7;
  output.left_trigger.force = 220;

  ds5_internal_output_report report = ds5_internal_build_usb_output_report(&output);
  assert(report.bytes[11] == DS5_TRIGGER_EFFECT_SECTION_RESISTANCE);
  assert(report.bytes[12] == 8);
  assert(report.bytes[13] == 140);
  assert(report.bytes[14] == 22);
  assert(report.bytes[22] == DS5_TRIGGER_EFFECT_WEAPON);
  assert(report.bytes[23] == 3);
  assert(report.bytes[24] == 220);
  assert(report.bytes[25] == 7);

  output.right_trigger.mode = DS5_TRIGGER_EFFECT_VIBRATION;
  output.right_trigger.frequency = 33;
  report = ds5_internal_build_usb_output_report(&output);
  assert(report.bytes[11] == DS5_TRIGGER_EFFECT_VIBRATION);
  assert(report.bytes[15] == 33);
}

static void test_touchpad_parser_decodes_two_fingers() {
  unsigned char report[64]{};
  report[0] = 0x01;
  report[8] = DS5_DPAD_NONE;
  report[30] = 0x05;
  report[31] = 0x34;
  report[32] = 0x12;
  report[33] = 0x56;
  report[34] = 0x86;
  report[35] = 0xbc;
  report[36] = 0x0a;
  report[37] = 0xde;

  ds5_state state{};
  ds5_result result = ds5_internal_parse_input_report(report, sizeof(report), DS5_TRANSPORT_USB, &state);
  assert(result == DS5_OK);
  assert(state.touch[0].active == 1u);
  assert(state.touch[0].id == 5u);
  assert(state.touch[0].x == 0x234u);
  assert(state.touch[0].y == 0x561u);
  assert(state.touch[1].active == 0u);
  assert(state.touch[1].id == 6u);
  assert(state.touch[1].x == 0xabcu);
  assert(state.touch[1].y == 0xde0u);
}

static void test_wav_pcm_loader_reads_commander_asset() {
  std::vector<uint8_t> pcm;
  ds5_audio_format format{};
  assert(ds5_tool_load_wav_pcm16("assets/commander_ready.wav", pcm, format));
  assert(!pcm.empty());
  assert(format.sample_rate == 48000u);
  assert(format.channels == 2u);
  assert(format.bits_per_sample == 16u);
}

static void test_ship_controls_map_dualsense_state_and_move_forward() {
  ds5_state state{};
  state.left_stick_x = 255;
  state.left_stick_y = 0;
  state.right_stick_x = 255;
  state.right_trigger = 0;
  state.left_trigger = 0;

  ShipInput input = ds5_demo_input_from_state(state);
  assert(input.strafe > 0.99f);
  assert(input.moveForward > 0.99f);
  assert(input.yaw == 0.0f);
  assert(input.pitch == 0.0f);
  assert(input.roll == 0.0f);
  assert(input.throttle == 0.0f);
  assert(input.brake == 0.0f);

  ShipPose pose{};
  ds5_demo_step_ship(pose, input, 0.5f);
  assert(pose.speed == 0.0f);
  assert(pose.z > 0.0f);
  assert(pose.x > 0.0f);
  assert(pose.yaw == 0.0f);

  ShipFeedback feedback = ds5_demo_feedback_from_ship(pose, input);
  assert(feedback.left_rumble == 0);
  assert(feedback.right_rumble == 0);
}

static void test_left_stick_right_strafes_ship_without_yawing() {
  ds5_state state{};
  state.left_stick_x = 255;
  state.left_stick_y = 128;

  ShipInput input = ds5_demo_input_from_state(state);
  assert(input.strafe > 0.99f);
  assert(input.moveForward == 0.0f);
  assert(input.throttle == 0.0f);
  assert(input.brake == 0.0f);
  assert(input.yaw == 0.0f);

  ShipPose pose{};
  ds5_demo_step_ship(pose, input, 0.5f);
  assert(pose.x > 0.0f);
  assert(std::fabs(pose.z) < 0.0001f);
  assert(pose.yaw == 0.0f);
}

static void test_left_stick_down_moves_ship_backward() {
  ds5_state state{};
  state.left_stick_x = 128;
  state.left_stick_y = 255;

  ShipInput input = ds5_demo_input_from_state(state);
  assert(input.pitch == 0.0f);
  assert(input.moveForward < -0.99f);
  assert(input.throttle == 0.0f);
  assert(input.brake == 0.0f);

  ShipPose pose{};
  ds5_demo_step_ship(pose, input, 0.5f);
  assert(pose.speed == 0.0f);
  assert(pose.z < 0.0f);
}

static void test_r2_only_fires_and_does_not_throttle_ship() {
  ds5_state soft{};
  soft.left_stick_x = 128;
  soft.left_stick_y = 128;
  soft.right_trigger = 32;

  ds5_state boost{};
  boost.left_stick_x = 128;
  boost.left_stick_y = 128;
  boost.right_trigger = 240;

  ShipInput soft_input = ds5_demo_input_from_state(soft);
  ShipInput boost_input = ds5_demo_input_from_state(boost);

  assert(soft_input.throttle == 0.0f);
  assert(boost_input.throttle == 0.0f);
  assert(soft_input.moveForward == 0.0f);
  assert(boost_input.moveForward == 0.0f);
}

static void test_r2_light_bullet_threshold_uses_raw_trigger() {
  assert(!ds5_demo_r2_fires_light_bullet(0.0f));
  assert(!ds5_demo_r2_fires_light_bullet(32.0f / 255.0f));
  assert(ds5_demo_r2_fires_light_bullet(128.0f / 255.0f));
}

static void test_light_bullet_spawns_from_ship_nose() {
  ShipPose pose{};
  ShipBullet forward = ds5_demo_spawn_light_bullet(pose);
  assert(forward.z > pose.z);
  assert(std::fabs(forward.x - pose.x) < 0.0001f);
  assert(forward.vz > 0.0f);

  pose.yaw = 1.5707963f;
  ShipBullet right = ds5_demo_spawn_light_bullet(pose);
  assert(right.x > pose.x);
  assert(std::fabs(right.vz) < 0.001f);

  pose.yaw = 0.0f;
  pose.pitch = 0.55f;
  ShipBullet down = ds5_demo_spawn_light_bullet(pose);
  assert(down.y < pose.y);
  assert(down.vy < 0.0f);
}

static void test_light_bullets_move_and_expire() {
  std::vector<ShipBullet> bullets;
  bullets.push_back(ds5_demo_spawn_light_bullet(ShipPose{}, 10.0f, 0.5f));
  ds5_demo_update_light_bullets(bullets, 0.25f);
  assert(bullets.size() == 1u);
  assert(bullets[0].z > 2.45f);
  ds5_demo_update_light_bullets(bullets, 0.25f);
  assert(bullets.empty());
}

static void test_ship_moves_along_heading_not_pitch() {
  ShipPose pose{};
  pose.pitch = 1.0f;
  pose.yaw = 1.5707963f;
  ShipInput input{};
  input.throttle = 1.0f;

  ds5_demo_step_ship(pose, input, 0.5f);
  assert(pose.speed > 0.0f);
  assert(pose.x > 0.0f);
  assert(std::fabs(pose.y) < 0.0001f);
  assert(std::fabs(pose.z) < 0.0001f);
}

static void test_left_stick_forward_uses_view_pitch_for_vertical_flight() {
  ShipPose pose{};
  ShipInput input{};
  input.moveForward = 1.0f;

  ds5_demo_step_ship_tuned(pose, input, 0.5f, 2.25f, 2.8f, 18.0f, 24.0f, 1.7f, 0.0f, 0.55f);

  assert(pose.y > 0.0f);
  assert(pose.z > 0.0f);
}

static void test_r2_thrust_uses_view_pitch_for_forward_flight() {
  ShipPose pose{};
  ShipInput input{};
  input.throttle = 1.0f;

  ds5_demo_step_ship_tuned(pose, input, 0.5f, 2.25f, 2.8f, 18.0f, 24.0f, 1.7f, 0.0f, 0.45f);

  assert(pose.speed > 0.0f);
  assert(pose.y > 0.0f);
  assert(pose.z > 0.0f);
}

static void test_neutral_view_pitch_keeps_left_stick_level() {
  ShipPose pose{};
  ShipInput input{};
  input.moveForward = 1.0f;

  ds5_demo_step_ship_tuned(pose, input, 0.5f, 2.25f, 2.8f, 18.0f, 24.0f, 1.7f, 0.0f, 0.0f);

  assert(std::fabs(pose.y) < 0.0001f);
  assert(pose.z > 0.0f);
}

static void test_engine_flame_only_when_flying_forward() {
  ShipPose pose{};
  ShipInput idle{};
  assert(ds5_demo_engine_flame_intensity(pose, idle) == 0.0f);

  ShipInput forward{};
  forward.moveForward = 0.7f;
  assert(ds5_demo_engine_flame_intensity(pose, forward) > 0.6f);

  ShipInput backward{};
  backward.moveForward = -1.0f;
  assert(ds5_demo_engine_flame_intensity(pose, backward) == 0.0f);

  pose.speed = 12.0f;
  assert(ds5_demo_engine_flame_intensity(pose, idle) > 0.4f);
}

static void test_camera_recenters_behind_ship_when_moving_without_manual_camera() {
  ShipControlConfig config{};
  ShipCameraState camera{};
  camera.yaw = 1.0f;
  camera.pitch = -0.5f;
  SpaceShipInputFrame input{};
  input.flight.throttle = 1.0f;

  ds5_demo_update_camera(camera, input, config, 0.2f);
  assert(camera.yaw < 1.0f);
  assert(camera.pitch > -0.5f);
}

static void test_camera_follow_lags_behind_direct_ship_movement() {
  ShipCameraState camera{};
  ShipPose pose{};
  ds5_demo_update_camera_follow(camera, pose, 0.016f);

  pose.x = 10.0f;
  ds5_demo_update_camera_follow(camera, pose, 0.016f);
  assert(camera.followX > 0.0f);
  assert(camera.followX < pose.x);
}

static void test_ship_feedback_increases_right_trigger_force_with_speed() {
  ShipPose slow{};
  slow.speed = 0.0f;
  ShipInput input{};
  input.throttle = 1.0f;
  ShipFeedback low = ds5_demo_feedback_from_ship(slow, input);

  ShipPose fast{};
  fast.speed = 24.0f;
  ShipFeedback high = ds5_demo_feedback_from_ship(fast, input);

  assert(low.right_trigger.mode == DS5_TRIGGER_EFFECT_CONSTANT_RESISTANCE);
  assert(high.right_trigger.force > low.right_trigger.force);
  assert(low.right_trigger.force >= 70);
}

static void test_ship_feedback_uses_light_rumble_and_near_zero_idle() {
  ShipPose idle_pose{};
  ShipInput idle_input{};
  ShipFeedback idle = ds5_demo_feedback_from_ship(idle_pose, idle_input);
  assert(idle.left_rumble == 0);
  assert(idle.right_rumble == 0);

  ShipPose fast{};
  fast.speed = 32.0f;
  ShipInput thrust{};
  thrust.throttle = 1.0f;
  ShipFeedback active = ds5_demo_feedback_from_ship(fast, thrust);
  assert(active.right_rumble > 0);
  assert(active.right_rumble <= 90);
}

static void test_r2_boost_feedback_is_stronger_than_cruise() {
  ShipPose pose{};
  ShipInput cruise{};
  cruise.throttle = 0.55f;
  ShipInput boost{};
  boost.throttle = 0.96f;

  ShipFeedback cruise_feedback = ds5_demo_feedback_from_ship(pose, cruise);
  ShipFeedback boost_feedback = ds5_demo_feedback_from_ship(pose, boost);

  assert(boost_feedback.right_trigger.force > cruise_feedback.right_trigger.force);
  assert(boost_feedback.right_rumble > cruise_feedback.right_rumble);
  assert(std::strcmp(boost_feedback.event_name, "boost") == 0);
}

static void test_ship_feedback_increases_left_trigger_force_with_brake() {
  ShipPose pose{};
  ShipInput coast{};
  ShipInput brake{};
  brake.brake = 1.0f;

  ShipFeedback low = ds5_demo_feedback_from_ship(pose, coast);
  ShipFeedback high = ds5_demo_feedback_from_ship(pose, brake);

  assert(high.left_trigger.force > low.left_trigger.force);
  assert(high.left_rumble > low.left_rumble);
}

static void test_trigger_mode_cycles_and_changes_r2_behavior() {
  ShipTriggerMode mode = ShipTriggerMode::Flight;
  mode = ds5_demo_next_trigger_mode(mode);
  assert(mode == ShipTriggerMode::Light);
  mode = ds5_demo_next_trigger_mode(mode);
  assert(mode == ShipTriggerMode::Heavy);
  mode = ds5_demo_next_trigger_mode(mode);
  assert(mode == ShipTriggerMode::Charge);
  mode = ds5_demo_next_trigger_mode(mode);
  assert(mode == ShipTriggerMode::Off);
  mode = ds5_demo_next_trigger_mode(mode);
  assert(mode == ShipTriggerMode::Flight);

  ShipPose pose{};
  pose.speed = 16.0f;
  ShipInput input{};
  input.throttle = 0.7f;
  ShipFeedback light = ds5_demo_feedback_from_ship(pose, input, ShipTriggerMode::Light);
  ShipFeedback heavy = ds5_demo_feedback_from_ship(pose, input, ShipTriggerMode::Heavy);
  ShipFeedback charge = ds5_demo_feedback_from_ship(pose, input, ShipTriggerMode::Charge);
  ShipFeedback off = ds5_demo_feedback_from_ship(pose, input, ShipTriggerMode::Off);
  assert(heavy.right_trigger.force > light.right_trigger.force);
  assert(charge.right_trigger.mode == DS5_TRIGGER_EFFECT_SECTION_RESISTANCE);
  assert(off.right_trigger.mode == DS5_TRIGGER_EFFECT_OFF);
}

static void test_r2_auto_fire_trigger_uses_vibration_effect() {
  ds5_trigger_effect effect = ds5_demo_r2_auto_fire_trigger_effect();
  assert(effect.mode == DS5_TRIGGER_EFFECT_VIBRATION);
  assert(effect.force >= 200);
  assert(effect.frequency >= 20);
  assert(effect.start_position < effect.end_position);

  ds5_trigger_effect rapid = ds5_demo_r2_auto_fire_trigger_effect(ShipFeedbackZoneKind::Rapid);
  ds5_trigger_effect heavy = ds5_demo_r2_auto_fire_trigger_effect(ShipFeedbackZoneKind::Heavy);
  assert(rapid.frequency > effect.frequency);
  assert(heavy.force > effect.force);
}

static void test_feedback_zones_detect_pose_and_change_feedback() {
  std::vector<ShipFeedbackZone> zones = {
      {ShipFeedbackZoneKind::Pulse, 4.0f, 0.0f, 0.0f, 2.0f},
      {ShipFeedbackZoneKind::Heavy, 12.0f, 0.0f, 0.0f, 2.0f},
  };
  ShipPose pose{};
  assert(ds5_demo_find_feedback_zone(pose, zones) == ShipFeedbackZoneKind::None);
  pose.x = 4.5f;
  assert(ds5_demo_find_feedback_zone(pose, zones) == ShipFeedbackZoneKind::Pulse);
  pose.x = 12.0f;
  assert(ds5_demo_find_feedback_zone(pose, zones) == ShipFeedbackZoneKind::Heavy);

  ShipFeedback feedback{};
  ds5_demo_apply_zone_feedback(feedback, ShipFeedbackZoneKind::Heavy);
  assert(feedback.right_rumble >= 90);
  assert(feedback.right_trigger.force >= 190);
}

static void test_target_lock_selects_target_inside_angle_and_distance() {
  ShipControlConfig config{};
  config.autoFollowMaxDistance = 50.0f;
  config.targetLockAngle = 25.0f;
  ShipPose pose{};
  std::vector<ShipTarget> targets = {
      {1, 0.0f, 0.0f, 25.0f, true},
      {2, 30.0f, 0.0f, 25.0f, true},
      {3, 0.0f, 0.0f, 80.0f, true},
  };
  assert(ds5_demo_find_lock_target(pose, targets, config) == 0);
}

static void test_target_lock_filters_close_targets_outside_angle() {
  ShipControlConfig config{};
  config.autoFollowMaxDistance = 100.0f;
  config.targetLockAngle = 20.0f;
  ShipPose pose{};
  std::vector<ShipTarget> targets = {
      {1, 8.0f, 0.0f, 0.0f, true},
      {2, 0.0f, 0.0f, 40.0f, true},
  };
  assert(ds5_demo_find_lock_target(pose, targets, config) == 1);
}

static void test_auto_follow_adds_assist_without_overriding_player() {
  ShipControlConfig config{};
  config.autoFollowStrength = 0.25f;
  ShipPose pose{};
  ShipTarget target{1, 10.0f, 0.0f, 20.0f, true};
  ShipInput player{};
  player.yaw = -0.2f;
  ShipDebugState debug{};
  ShipInput assisted = ds5_demo_apply_auto_follow(pose, player, &target, config, debug);
  assert(assisted.yaw > player.yaw);
  assert(assisted.yaw < 1.0f);
  assert(debug.followStrength == config.autoFollowStrength);
}

static void test_motion_control_roll_and_dodge_can_be_disabled() {
  ShipControlConfig config{};
  config.motionControlEnabled = true;
  config.gyroSensitivity = 0.001f;
  config.motionDodgeThreshold = 1000.0f;
  ds5_state state{};
  state.gyro_x = 1500;
  state.gyro_y = -1200;
  state.gyro_z = 2000;
  state.accel_y = 8000;
  state.accel_z = 1200;
  ShipMotionControl motion{};
  ShipInput attitude = motion.updateAttitude(state, config);
  assert(attitude.pitch > 0.0f);
  assert(attitude.yaw > 0.0f);
  assert(attitude.roll > 0.0f);
  assert(motion.detectDodge(state, config));

  ds5_state baseline{};
  baseline.accel_y = 8000;
  baseline.accel_z = 1200;
  ShipMotionControl tilt_motion{};
  tilt_motion.updateAttitude(baseline, config);
  ds5_state tilted = baseline;
  tilted.accel_x = 2400;
  tilted.accel_z = 3200;
  ShipInput tilt_attitude = tilt_motion.updateAttitude(tilted, config);
  assert(tilt_attitude.pitch > 0.0f);
  assert(tilt_attitude.roll > 0.0f);

  ShipControlConfig disabled = config;
  disabled.motionControlEnabled = false;
  ShipMotionControl disabled_motion{};
  ShipInput disabled_attitude = disabled_motion.updateAttitude(state, disabled);
  assert(disabled_attitude.pitch == 0.0f);
  assert(disabled_attitude.yaw == 0.0f);
  assert(disabled_attitude.roll == 0.0f);
  assert(!disabled_motion.detectDodge(state, disabled));
}

static void test_motion_axis_mapping_is_configurable() {
  ShipControlConfig config{};
  config.motionControlEnabled = true;
  config.gyroSensitivity = 0.001f;
  config.gyroSmoothing = 1.0f;
  config.motionPitchGyro = ds5_demo_parse_motion_axis("z");
  config.motionYawGyro = ds5_demo_parse_motion_axis("y");
  config.motionRollGyro = ds5_demo_parse_motion_axis("y");
  config.motionPitchAccel = ds5_demo_parse_motion_axis("-x");
  config.motionRollAccel = ds5_demo_parse_motion_axis("z");

  ds5_state state{};
  state.gyro_x = -200;
  state.gyro_y = -300;
  state.gyro_z = 400;
  state.accel_y = 8000;
  state.accel_z = 1000;

  ShipMotionControl motion{};
  motion.updateAttitude(state, config);
  state.accel_x = -500;
  state.accel_z = 1500;
  ShipInput input = motion.updateAttitude(state, config);
  assert(input.pitch > 0.0f);
  assert(input.yaw > 0.0f);
  assert(input.roll > 0.0f);
}

static void test_motion_calibrates_static_gyro_bias() {
  ShipControlConfig config{};
  config.motionControlEnabled = true;
  config.gyroSensitivity = 0.001f;
  config.gyroSmoothing = 1.0f;
  config.motionInputDeadzone = 0.01f;
  config.gyroCalibrationTime = 0.2f;
  config.gyroAutoCalibrationTime = 0.2f;
  config.gyroStillnessGyroDelta = 4.0f;
  config.gyroStillnessAccelDelta = 20.0f;

  ds5_state state{};
  state.gyro_x = 30;
  state.gyro_y = -24;
  state.gyro_z = 18;
  state.accel_y = 8000;
  state.accel_z = 1200;

  ShipMotionControl motion{};
  for (int i = 0; i < 20; ++i) {
    motion.updateAttitude(state, config, 0.02f);
  }
  ShipInput calibrated = motion.updateAttitude(state, config, 0.02f);
  assert(std::fabs(motion.gyroBiasX - 30.0f) < 0.1f);
  assert(std::fabs(motion.gyroBiasY + 24.0f) < 0.1f);
  assert(std::fabs(motion.gyroBiasZ - 18.0f) < 0.1f);
  assert(calibrated.pitch == 0.0f);
  assert(calibrated.yaw == 0.0f);
  assert(calibrated.roll == 0.0f);
}

int main() {
  try {
    run_test("abi_structs_are_versioned", test_abi_structs_are_versioned);
    run_test("public_helpers_report_version_and_results", test_public_helpers_report_version_and_results);
    run_test("public_trigger_builders", test_public_trigger_builders);
    run_test("public_struct_version_is_enforced", test_public_struct_version_is_enforced);
    run_test("poll_timeout_validates_arguments", test_poll_timeout_validates_arguments);
    run_test("reset_feedback_clears_cached_output_before_transport_check", test_reset_feedback_clears_cached_output_before_transport_check);
    run_test("reset_feedback_rejects_null_device", test_reset_feedback_rejects_null_device);
    run_test("reset_feedback_output_encoding_is_clear", test_reset_feedback_output_encoding_is_clear);
    run_test("capabilities_for_usb_are_full_featured", test_capabilities_for_usb_are_full_featured);
    run_test("capabilities_for_bluetooth_are_reduced", test_capabilities_for_bluetooth_are_reduced);
    run_test("usb_input_report_parser", test_usb_input_report_parser);
    run_test("bluetooth_short_input_report_parser", test_bluetooth_short_input_report_parser);
    run_test("bluetooth_output_reports_are_rejected_before_io", test_bluetooth_output_reports_are_rejected_before_io);
    run_test("output_report_builder_light_rumble_trigger", test_output_report_builder_light_rumble_trigger);
    run_test("output_report_builder_encodes_all_public_trigger_modes", test_output_report_builder_encodes_all_public_trigger_modes);
    run_test("touchpad_parser_decodes_two_fingers", test_touchpad_parser_decodes_two_fingers);
    run_test("wav_pcm_loader_reads_commander_asset", test_wav_pcm_loader_reads_commander_asset);
    run_test("ship_controls_map_dualsense_state_and_move_forward", test_ship_controls_map_dualsense_state_and_move_forward);
    run_test("left_stick_right_strafes_ship_without_yawing", test_left_stick_right_strafes_ship_without_yawing);
    run_test("left_stick_down_moves_ship_backward", test_left_stick_down_moves_ship_backward);
    run_test("r2_only_fires_and_does_not_throttle_ship", test_r2_only_fires_and_does_not_throttle_ship);
    run_test("r2_light_bullet_threshold_uses_raw_trigger", test_r2_light_bullet_threshold_uses_raw_trigger);
    run_test("light_bullet_spawns_from_ship_nose", test_light_bullet_spawns_from_ship_nose);
    run_test("light_bullets_move_and_expire", test_light_bullets_move_and_expire);
    run_test("ship_moves_along_heading_not_pitch", test_ship_moves_along_heading_not_pitch);
    run_test("left_stick_forward_uses_view_pitch_for_vertical_flight", test_left_stick_forward_uses_view_pitch_for_vertical_flight);
    run_test("r2_thrust_uses_view_pitch_for_forward_flight", test_r2_thrust_uses_view_pitch_for_forward_flight);
    run_test("neutral_view_pitch_keeps_left_stick_level", test_neutral_view_pitch_keeps_left_stick_level);
    run_test("engine_flame_only_when_flying_forward", test_engine_flame_only_when_flying_forward);
    run_test("camera_recenters_behind_ship_when_moving_without_manual_camera", test_camera_recenters_behind_ship_when_moving_without_manual_camera);
    run_test("camera_follow_lags_behind_direct_ship_movement", test_camera_follow_lags_behind_direct_ship_movement);
    run_test("ship_feedback_increases_right_trigger_force_with_speed", test_ship_feedback_increases_right_trigger_force_with_speed);
    run_test("ship_feedback_uses_light_rumble_and_near_zero_idle", test_ship_feedback_uses_light_rumble_and_near_zero_idle);
    run_test("r2_boost_feedback_is_stronger_than_cruise", test_r2_boost_feedback_is_stronger_than_cruise);
    run_test("ship_feedback_increases_left_trigger_force_with_brake", test_ship_feedback_increases_left_trigger_force_with_brake);
    run_test("trigger_mode_cycles_and_changes_r2_behavior", test_trigger_mode_cycles_and_changes_r2_behavior);
    run_test("r2_auto_fire_trigger_uses_vibration_effect", test_r2_auto_fire_trigger_uses_vibration_effect);
    run_test("feedback_zones_detect_pose_and_change_feedback", test_feedback_zones_detect_pose_and_change_feedback);
    run_test("target_lock_selects_target_inside_angle_and_distance", test_target_lock_selects_target_inside_angle_and_distance);
    run_test("target_lock_filters_close_targets_outside_angle", test_target_lock_filters_close_targets_outside_angle);
    run_test("auto_follow_adds_assist_without_overriding_player", test_auto_follow_adds_assist_without_overriding_player);
    run_test("motion_control_roll_and_dodge_can_be_disabled", test_motion_control_roll_and_dodge_can_be_disabled);
    run_test("motion_axis_mapping_is_configurable", test_motion_axis_mapping_is_configurable);
    run_test("motion_calibrates_static_gyro_bias", test_motion_calibrates_static_gyro_bias);
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }

  std::cout << "dualsense_tests passed\n";
  return 0;
}
