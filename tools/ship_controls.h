/*
  File: ship_controls.h
  Author: Herbert Yeung
  Purpose: Ship demo input mapping helpers for DualSense and fallback controls.
*/

#ifndef DS5_SHIP_CONTROLS_H
#define DS5_SHIP_CONTROLS_H

#include <algorithm>
#include <cmath>

#include <dualsense/dualsense.h>

struct ShipPose {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float pitch = 0.0f;
  float yaw = 0.0f;
  float roll = 0.0f;
  float speed = 0.0f;
};

struct ShipInput {
  float pitch = 0.0f;
  float yaw = 0.0f;
  float roll = 0.0f;
  float strafe = 0.0f;
  float moveForward = 0.0f;
  float throttle = 0.0f;
  float brake = 0.0f;
};

inline float ds5_demo_clamp(float value, float minimum, float maximum) {
  return std::max(minimum, std::min(maximum, value));
}

inline float ds5_demo_axis_from_byte(unsigned char value, bool invert = false) {
  float axis = (static_cast<float>(value) - 128.0f) / 127.0f;
  axis = ds5_demo_clamp(axis, -1.0f, 1.0f);
  return invert ? -axis : axis;
}

inline float ds5_demo_r2_propulsion_curve(float raw_trigger) {
  const float raw = ds5_demo_clamp(raw_trigger, 0.0f, 1.0f);
  if (raw < 0.04f) return 0.0f;
  const float t = (raw - 0.04f) / 0.96f;
  if (t < 0.35f) {
    return t * t * 0.55f;
  }
  if (t < 0.82f) {
    const float cruise = (t - 0.35f) / 0.47f;
    return 0.067f + cruise * 0.68f;
  }
  const float boost = (t - 0.82f) / 0.18f;
  return ds5_demo_clamp(0.747f + boost * 0.253f, 0.0f, 1.0f);
}

inline bool ds5_demo_r2_fires_light_bullet(float raw_trigger) {
  return raw_trigger > 0.18f;
}

inline ShipInput ds5_demo_input_from_state(const ds5_state& state) {
  ShipInput input{};
  input.strafe = ds5_demo_axis_from_byte(state.left_stick_x);
  input.moveForward = ds5_demo_axis_from_byte(state.left_stick_y, true);
  input.brake = static_cast<float>(state.left_trigger) / 255.0f;
  return input;
}

inline void ds5_demo_step_ship(ShipPose& pose, const ShipInput& input, float dt) {
  const float turn_rate = 2.25f;
  const float roll_rate = 2.8f;
  const float acceleration = 18.0f;
  const float drag = 1.8f;

  pose.yaw += input.yaw * turn_rate * dt;
  pose.pitch += input.pitch * turn_rate * dt;
  pose.roll += input.roll * roll_rate * dt;
  pose.pitch = ds5_demo_clamp(pose.pitch, -1.25f, 1.25f);

  pose.speed += (input.throttle - input.brake) * acceleration * dt;
  pose.speed -= pose.speed * drag * dt;
  pose.speed = ds5_demo_clamp(pose.speed, -8.0f, 32.0f);

  const float cy = std::cos(pose.yaw);
  const float sy = std::sin(pose.yaw);

  pose.x += (sy * pose.speed + cy * input.strafe * acceleration + sy * input.moveForward * acceleration) * dt;
  pose.z += (cy * pose.speed - sy * input.strafe * acceleration + cy * input.moveForward * acceleration) * dt;
}

inline void ds5_demo_step_ship_tuned(ShipPose& pose, const ShipInput& input, float dt,
                                     float turn_rate, float roll_rate, float acceleration,
                                     float brake_strength, float boost_multiplier,
                                     float view_yaw_offset = 0.0f, float view_pitch = 0.0f) {
  pose.yaw += input.yaw * turn_rate * dt;
  pose.pitch += input.pitch * turn_rate * dt;
  pose.roll += input.roll * roll_rate * dt;
  pose.pitch = ds5_demo_clamp(pose.pitch, -1.25f, 1.25f);

  const float thrust = input.throttle * acceleration * (input.throttle > 0.9f ? boost_multiplier : 1.0f);
  pose.speed += thrust * dt;
  pose.speed -= input.brake * brake_strength * dt;
  pose.speed -= pose.speed * 1.35f * dt;
  pose.speed = ds5_demo_clamp(pose.speed, -8.0f, 36.0f * boost_multiplier);

  const float movement_yaw = pose.yaw + view_yaw_offset;
  const float cy = std::cos(movement_yaw);
  const float sy = std::sin(movement_yaw);
  const float cp = std::cos(view_pitch);
  const float sp = std::sin(view_pitch);
  const float forward_x = sy * cp;
  const float forward_y = sp;
  const float forward_z = cy * cp;
  const float right_x = cy;
  const float right_z = -sy;
  pose.x += (forward_x * pose.speed + right_x * input.strafe * acceleration + forward_x * input.moveForward * acceleration) * dt;
  pose.y += (forward_y * pose.speed + forward_y * input.moveForward * acceleration) * dt;
  pose.z += (forward_z * pose.speed + right_z * input.strafe * acceleration + forward_z * input.moveForward * acceleration) * dt;
}

#endif
