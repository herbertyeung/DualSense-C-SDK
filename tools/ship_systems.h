/*
  File: ship_systems.h
  Author: Herbert Yeung
  Purpose: Ship demo movement, camera, target, projectile, and motion systems.
*/

#ifndef DS5_SHIP_SYSTEMS_H
#define DS5_SHIP_SYSTEMS_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "ship_config.h"
#include "ship_controls.h"

struct ShipCameraState {
  float yaw = 0.0f;
  float pitch = 0.2f;
  float followX = 0.0f;
  float followY = 0.0f;
  float followZ = 0.0f;
  bool hasFollowTarget = false;
};

struct ShipTarget {
  int id = 0;
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  bool alive = true;
};

struct ShipDebugState {
  int lockedTargetId = -1;
  float lockedDistance = 0.0f;
  float lockedAngle = 0.0f;
  float followStrength = 0.0f;
  bool autoFollow = false;
  bool adaptiveTrigger = false;
  bool gyro = false;
  bool gyroSteady = false;
  float gyroCalibrationConfidence = 0.0f;
  float gyroBiasX = 0.0f;
  float gyroBiasY = 0.0f;
  float gyroBiasZ = 0.0f;
  std::string lastFeedback = "none";
};

struct SpaceShipInputFrame {
  ShipInput flight{};
  float cameraYaw = 0.0f;
  float cameraPitch = 0.0f;
  bool boost = false;
  bool dodge = false;
  bool fire = false;
  bool switchWeapon = false;
  bool lockTarget = false;
  bool toggleAutoFollow = false;
  bool pause = false;
  bool touchpad = false;
};

struct ShipBullet {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float vx = 0.0f;
  float vy = 0.0f;
  float vz = 0.0f;
  float age = 0.0f;
  float lifetime = 1.25f;
};

enum class ShipFeedbackZoneKind {
  None = 0,
  Pulse = 1,
  Heavy = 2,
  Rapid = 3,
  Sticky = 4,
};

struct ShipFeedbackZone {
  ShipFeedbackZoneKind kind = ShipFeedbackZoneKind::None;
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float half_extent = 4.0f;
};

inline float ds5_demo_apply_deadzone(float value, float deadzone) {
  if (std::fabs(value) < deadzone) return 0.0f;
  const float sign = value < 0.0f ? -1.0f : 1.0f;
  return sign * ((std::fabs(value) - deadzone) / (1.0f - deadzone));
}

inline float ds5_demo_zero_small(float value, float deadzone) {
  return std::fabs(value) < deadzone ? 0.0f : value;
}

inline ShipBullet ds5_demo_spawn_light_bullet(const ShipPose& pose, float speed = 64.0f, float lifetime = 1.25f) {
  const float cp = std::cos(pose.pitch);
  const float fx = std::sin(pose.yaw) * cp;
  const float fy = -std::sin(pose.pitch);
  const float fz = std::cos(pose.yaw) * cp;
  ShipBullet bullet{};
  bullet.x = pose.x + fx * 2.45f;
  bullet.y = pose.y + fy * 2.45f + 0.06f;
  bullet.z = pose.z + fz * 2.45f;
  bullet.vx = fx * speed;
  bullet.vy = fy * speed;
  bullet.vz = fz * speed;
  bullet.lifetime = lifetime;
  return bullet;
}

inline void ds5_demo_update_light_bullets(std::vector<ShipBullet>& bullets, float dt) {
  for (ShipBullet& bullet : bullets) {
    bullet.x += bullet.vx * dt;
    bullet.y += bullet.vy * dt;
    bullet.z += bullet.vz * dt;
    bullet.age += dt;
  }
  bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [](const ShipBullet& bullet) {
                  return bullet.age >= bullet.lifetime;
                }),
                bullets.end());
}

inline float ds5_demo_engine_flame_intensity(const ShipPose& pose, const ShipInput& input) {
  const float forward_input = ds5_demo_clamp(input.moveForward, 0.0f, 1.0f);
  const float forward_speed = ds5_demo_clamp(pose.speed / 24.0f, 0.0f, 1.0f);
  return ds5_demo_clamp(std::max(forward_input, forward_speed), 0.0f, 1.0f);
}

inline const char* ds5_demo_feedback_zone_name(ShipFeedbackZoneKind zone) {
  switch (zone) {
    case ShipFeedbackZoneKind::Pulse: return "pulse";
    case ShipFeedbackZoneKind::Heavy: return "heavy";
    case ShipFeedbackZoneKind::Rapid: return "rapid";
    case ShipFeedbackZoneKind::Sticky: return "sticky";
    default: return "none";
  }
}

inline ShipFeedbackZoneKind ds5_demo_find_feedback_zone(const ShipPose& pose, const std::vector<ShipFeedbackZone>& zones) {
  for (const ShipFeedbackZone& zone : zones) {
    const float e = zone.half_extent;
    if (pose.x >= zone.x - e && pose.x <= zone.x + e &&
        pose.y >= zone.y - e && pose.y <= zone.y + e &&
        pose.z >= zone.z - e && pose.z <= zone.z + e) {
      return zone.kind;
    }
  }
  return ShipFeedbackZoneKind::None;
}

inline void ds5_demo_update_camera(ShipCameraState& camera, const SpaceShipInputFrame& input, const ShipControlConfig& config, float dt) {
  camera.yaw += input.cameraYaw * config.turnSpeed * dt;
  camera.pitch += input.cameraPitch * config.turnSpeed * dt;
  const bool camera_is_manual = std::fabs(input.cameraYaw) > 0.02f || std::fabs(input.cameraPitch) > 0.02f;
  const bool ship_is_moving =
      input.flight.throttle > 0.05f ||
      input.flight.brake > 0.05f ||
      std::fabs(input.flight.strafe) > 0.05f ||
      std::fabs(input.flight.moveForward) > 0.05f;
  if (!camera_is_manual && ship_is_moving) {
    const float follow = ds5_demo_clamp(dt * 2.5f, 0.0f, 1.0f);
    camera.yaw += (0.0f - camera.yaw) * follow;
    camera.pitch += (0.2f - camera.pitch) * follow;
  }
  camera.pitch = ds5_demo_clamp(camera.pitch, -1.1f, 1.1f);
}

inline void ds5_demo_update_camera_follow(ShipCameraState& camera, const ShipPose& pose, float dt) {
  if (!camera.hasFollowTarget) {
    camera.followX = pose.x;
    camera.followY = pose.y;
    camera.followZ = pose.z;
    camera.hasFollowTarget = true;
    return;
  }
  const float follow = ds5_demo_clamp(dt * 5.5f, 0.0f, 1.0f);
  camera.followX += (pose.x - camera.followX) * follow;
  camera.followY += (pose.y - camera.followY) * follow;
  camera.followZ += (pose.z - camera.followZ) * follow;
}

inline float ds5_demo_distance_to_target(const ShipPose& pose, const ShipTarget& target) {
  const float dx = target.x - pose.x;
  const float dy = target.y - pose.y;
  const float dz = target.z - pose.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

inline float ds5_demo_angle_to_target_degrees(const ShipPose& pose, const ShipTarget& target) {
  const float dx = target.x - pose.x;
  const float dy = target.y - pose.y;
  const float dz = target.z - pose.z;
  const float len = std::max(0.001f, std::sqrt(dx * dx + dy * dy + dz * dz));
  const float cp = std::cos(pose.pitch);
  const float fx = std::sin(pose.yaw) * cp;
  const float fy = std::sin(pose.pitch);
  const float fz = std::cos(pose.yaw) * cp;
  const float dot = ds5_demo_clamp((fx * dx + fy * dy + fz * dz) / len, -1.0f, 1.0f);
  return std::acos(dot) * 57.2957795f;
}

inline int ds5_demo_find_lock_target(const ShipPose& pose, const std::vector<ShipTarget>& targets, const ShipControlConfig& config) {
  int best = -1;
  float best_score = 1.0e9f;
  const float max_distance_sq = config.autoFollowMaxDistance * config.autoFollowMaxDistance;
  const float cp = std::cos(pose.pitch);
  const float fx = std::sin(pose.yaw) * cp;
  const float fy = std::sin(pose.pitch);
  const float fz = std::cos(pose.yaw) * cp;
  const float min_dot = std::cos(config.targetLockAngle / 57.2957795f);
  for (size_t i = 0; i < targets.size(); ++i) {
    if (!targets[i].alive) continue;
    const float dx = targets[i].x - pose.x;
    const float dy = targets[i].y - pose.y;
    const float dz = targets[i].z - pose.z;
    const float distance_sq = dx * dx + dy * dy + dz * dz;
    if (distance_sq > max_distance_sq) continue;

    const float distance = std::max(0.001f, std::sqrt(distance_sq));
    const float dot = ds5_demo_clamp((fx * dx + fy * dy + fz * dz) / distance, -1.0f, 1.0f);
    if (dot < min_dot) continue;

    const float angle = std::acos(dot) * 57.2957795f;
    const float score = angle * 2.0f + distance * 0.05f;
    if (score < best_score) {
      best_score = score;
      best = static_cast<int>(i);
    }
  }
  return best;
}

inline ShipInput ds5_demo_apply_auto_follow(const ShipPose& pose, const ShipInput& player, const ShipTarget* target,
                                            const ShipControlConfig& config, ShipDebugState& debug) {
  if (!target || !target->alive) {
    debug.followStrength = 0.0f;
    return player;
  }

  const float dx = target->x - pose.x;
  const float dy = target->y - pose.y;
  const float dz = target->z - pose.z;
  const float desired_yaw = std::atan2(dx, dz);
  const float flat = std::sqrt(dx * dx + dz * dz);
  const float desired_pitch = std::atan2(dy, flat);
  auto wrap = [](float v) {
    while (v > 3.14159265f) v -= 6.2831853f;
    while (v < -3.14159265f) v += 6.2831853f;
    return v;
  };

  const float yaw_error = ds5_demo_clamp(wrap(desired_yaw - pose.yaw), -1.0f, 1.0f);
  const float pitch_error = ds5_demo_clamp(desired_pitch - pose.pitch, -1.0f, 1.0f);
  ShipInput output = player;
  output.yaw = ds5_demo_clamp(player.yaw + yaw_error * config.autoFollowStrength, -1.0f, 1.0f);
  output.pitch = ds5_demo_clamp(player.pitch + pitch_error * config.autoFollowStrength, -1.0f, 1.0f);
  debug.followStrength = config.autoFollowStrength;
  return output;
}

struct ShipMotionControl {
  float smoothedPitch = 0.0f;
  float smoothedYaw = 0.0f;
  float smoothedRoll = 0.0f;
  float gyroBiasX = 0.0f;
  float gyroBiasY = 0.0f;
  float gyroBiasZ = 0.0f;
  float minGyroX = 0.0f;
  float minGyroY = 0.0f;
  float minGyroZ = 0.0f;
  float maxGyroX = 0.0f;
  float maxGyroY = 0.0f;
  float maxGyroZ = 0.0f;
  float minAccelX = 0.0f;
  float minAccelY = 0.0f;
  float minAccelZ = 0.0f;
  float maxAccelX = 0.0f;
  float maxAccelY = 0.0f;
  float maxAccelZ = 0.0f;
  float stillnessTime = 0.0f;
  float calibrationTime = 0.0f;
  float calibrationConfidence = 0.0f;
  float accelBaseX = 0.0f;
  float accelBaseY = 0.0f;
  float accelBaseZ = 0.0f;
  bool hasAccelBase = false;
  bool hasStillnessWindow = false;
  bool gyroSteady = false;
  int dodgeCooldownFrames = 0;

  static float selectAxis(const ds5_state& state, const ShipMotionAxisBinding& binding, bool gyro) {
    float value = 0.0f;
    switch (binding.axis) {
      case ShipMotionAxis::X:
        value = static_cast<float>(gyro ? state.gyro_x : state.accel_x);
        break;
      case ShipMotionAxis::Y:
        value = static_cast<float>(gyro ? state.gyro_y : state.accel_y);
        break;
      case ShipMotionAxis::Z:
        value = static_cast<float>(gyro ? state.gyro_z : state.accel_z);
        break;
      case ShipMotionAxis::None:
      default:
        value = 0.0f;
        break;
    }
    return value * binding.sign;
  }

  static float selectAxis(float x, float y, float z, const ShipMotionAxisBinding& binding) {
    float value = 0.0f;
    switch (binding.axis) {
      case ShipMotionAxis::X:
        value = x;
        break;
      case ShipMotionAxis::Y:
        value = y;
        break;
      case ShipMotionAxis::Z:
        value = z;
        break;
      case ShipMotionAxis::None:
      default:
        value = 0.0f;
        break;
    }
    return value * binding.sign;
  }

  void resetStillnessWindow(const ds5_state& state) {
    minGyroX = maxGyroX = static_cast<float>(state.gyro_x);
    minGyroY = maxGyroY = static_cast<float>(state.gyro_y);
    minGyroZ = maxGyroZ = static_cast<float>(state.gyro_z);
    minAccelX = maxAccelX = static_cast<float>(state.accel_x);
    minAccelY = maxAccelY = static_cast<float>(state.accel_y);
    minAccelZ = maxAccelZ = static_cast<float>(state.accel_z);
    stillnessTime = 0.0f;
    hasStillnessWindow = true;
    gyroSteady = false;
  }

  void updateStillnessWindowRanges(const ds5_state& state) {
    const float gx = static_cast<float>(state.gyro_x);
    const float gy = static_cast<float>(state.gyro_y);
    const float gz = static_cast<float>(state.gyro_z);
    const float ax = static_cast<float>(state.accel_x);
    const float ay = static_cast<float>(state.accel_y);
    const float az = static_cast<float>(state.accel_z);
    minGyroX = std::min(minGyroX, gx);
    minGyroY = std::min(minGyroY, gy);
    minGyroZ = std::min(minGyroZ, gz);
    maxGyroX = std::max(maxGyroX, gx);
    maxGyroY = std::max(maxGyroY, gy);
    maxGyroZ = std::max(maxGyroZ, gz);
    minAccelX = std::min(minAccelX, ax);
    minAccelY = std::min(minAccelY, ay);
    minAccelZ = std::min(minAccelZ, az);
    maxAccelX = std::max(maxAccelX, ax);
    maxAccelY = std::max(maxAccelY, ay);
    maxAccelZ = std::max(maxAccelZ, az);
  }

  bool stillnessWindowIsQuiet(const ShipControlConfig& config) const {
    const bool gyroQuiet =
        (maxGyroX - minGyroX) <= config.gyroStillnessGyroDelta &&
        (maxGyroY - minGyroY) <= config.gyroStillnessGyroDelta &&
        (maxGyroZ - minGyroZ) <= config.gyroStillnessGyroDelta;
    const bool accelQuiet =
        (maxAccelX - minAccelX) <= config.gyroStillnessAccelDelta &&
        (maxAccelY - minAccelY) <= config.gyroStillnessAccelDelta &&
        (maxAccelZ - minAccelZ) <= config.gyroStillnessAccelDelta;
    return gyroQuiet && accelQuiet;
  }

  void applyStillnessBiasCalibration(const ShipControlConfig& config, float dt, float requiredTime) {
    const float targetBiasX = (minGyroX + maxGyroX) * 0.5f;
    const float targetBiasY = (minGyroY + maxGyroY) * 0.5f;
    const float targetBiasZ = (minGyroZ + maxGyroZ) * 0.5f;
    const float blend = calibrationConfidence < 0.05f
                            ? 1.0f
                            : ds5_demo_clamp(dt * config.gyroBiasLerpSpeed * (0.35f + calibrationConfidence), 0.0f, 1.0f);
    gyroBiasX += (targetBiasX - gyroBiasX) * blend;
    gyroBiasY += (targetBiasY - gyroBiasY) * blend;
    gyroBiasZ += (targetBiasZ - gyroBiasZ) * blend;
    calibrationConfidence = ds5_demo_clamp(calibrationConfidence + dt / std::max(0.1f, requiredTime), 0.0f, 1.0f);
  }

  void updateGyroCalibration(const ds5_state& state, const ShipControlConfig& config, float dt) {
    if (dt <= 0.0f) return;
    if (!hasStillnessWindow) {
      resetStillnessWindow(state);
      return;
    }

    updateStillnessWindowRanges(state);
    stillnessTime += dt;
    calibrationTime += dt;

    gyroSteady = stillnessWindowIsQuiet(config);
    const float requiredTime = calibrationConfidence < 0.2f ? config.gyroCalibrationTime : config.gyroAutoCalibrationTime;
    if (gyroSteady && stillnessTime >= requiredTime) {
      applyStillnessBiasCalibration(config, dt, requiredTime);
      resetStillnessWindow(state);
    } else if (!gyroSteady && stillnessTime > 0.2f) {
      resetStillnessWindow(state);
    }
  }

  ShipInput updateAttitude(const ds5_state& state, const ShipControlConfig& config, float dt = 1.0f / 60.0f) {
    ShipInput input{};
    if (!config.motionControlEnabled) return input;
    updateGyroCalibration(state, config, dt);
    const float ax = static_cast<float>(state.accel_x);
    const float ay = static_cast<float>(state.accel_y);
    const float az = static_cast<float>(state.accel_z);
    const float accelMagnitude = std::sqrt(ax * ax + ay * ay + az * az);
    if (!hasAccelBase && accelMagnitude > 1000.0f) {
      accelBaseX = ax;
      accelBaseY = ay;
      accelBaseZ = az;
      hasAccelBase = true;
    }

    const float accelScale = config.gyroSensitivity * 2.2f;
    const float dax = ax - accelBaseX;
    const float day = ay - accelBaseY;
    const float daz = az - accelBaseZ;
    const float accelPitch = hasAccelBase ? selectAxis(dax, day, daz, config.motionPitchAccel) * accelScale : 0.0f;
    const float accelRoll = hasAccelBase ? selectAxis(dax, day, daz, config.motionRollAccel) * accelScale : 0.0f;
    ds5_state calibrated = state;
    calibrated.gyro_x = static_cast<int16_t>(static_cast<float>(state.gyro_x) - gyroBiasX);
    calibrated.gyro_y = static_cast<int16_t>(static_cast<float>(state.gyro_y) - gyroBiasY);
    calibrated.gyro_z = static_cast<int16_t>(static_cast<float>(state.gyro_z) - gyroBiasZ);
    const float gravLen = std::max(1.0f, accelMagnitude);
    const float gravY = ay / gravLen;
    const float gravZ = az / gravLen;
    const float calibratedGyroY = static_cast<float>(calibrated.gyro_y);
    const float calibratedGyroZ = static_cast<float>(calibrated.gyro_z);
    const float playerSpaceYaw = -(gravY * calibratedGyroY + gravZ * calibratedGyroZ);
    const float yawSign = playerSpaceYaw < 0.0f ? -1.0f : 1.0f;
    const float relaxedYaw = yawSign * std::min(std::fabs(playerSpaceYaw) * 1.41f,
                                                std::sqrt(calibratedGyroY * calibratedGyroY + calibratedGyroZ * calibratedGyroZ));
    const float gyroPitch = selectAxis(calibrated, config.motionPitchGyro, true) * config.gyroSensitivity * 0.65f;
    const float rawYaw = relaxedYaw * config.gyroSensitivity * 1.10f;
    const float gyroRoll = selectAxis(calibrated, config.motionRollGyro, true) * config.gyroSensitivity * 0.65f;
    const float rawPitch = accelPitch + gyroPitch;
    const float rawRoll = accelRoll + gyroRoll;
    smoothedPitch += (rawPitch - smoothedPitch) * config.gyroSmoothing;
    smoothedYaw += (rawYaw - smoothedYaw) * config.gyroSmoothing;
    smoothedRoll += (rawRoll - smoothedRoll) * config.gyroSmoothing;
    input.pitch = ds5_demo_zero_small(ds5_demo_clamp(smoothedPitch, -0.35f, 0.35f), config.motionInputDeadzone);
    input.yaw = ds5_demo_zero_small(ds5_demo_clamp(smoothedYaw, -0.25f, 0.25f), config.motionInputDeadzone);
    input.roll = ds5_demo_zero_small(ds5_demo_clamp(smoothedRoll, -0.45f, 0.45f), config.motionInputDeadzone);
    return input;
  }

  float updateRoll(const ds5_state& state, const ShipControlConfig& config) {
    return updateAttitude(state, config).roll;
  }

  void writeDebug(ShipDebugState& debug) const {
    debug.gyroSteady = gyroSteady;
    debug.gyroCalibrationConfidence = calibrationConfidence;
    debug.gyroBiasX = gyroBiasX;
    debug.gyroBiasY = gyroBiasY;
    debug.gyroBiasZ = gyroBiasZ;
  }

  bool detectDodge(const ds5_state& state, const ShipControlConfig& config) {
    if (!config.motionControlEnabled) return false;
    if (dodgeCooldownFrames > 0) {
      --dodgeCooldownFrames;
      return false;
    }
    if (std::fabs(static_cast<float>(state.gyro_z)) > config.motionDodgeThreshold) {
      dodgeCooldownFrames = 28;
      return true;
    }
    return false;
  }
};

#endif
