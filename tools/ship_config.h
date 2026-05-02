#ifndef DS5_SHIP_CONFIG_H
#define DS5_SHIP_CONFIG_H

#include <cstdlib>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

enum class ShipMotionAxis {
  None = 0,
  X = 1,
  Y = 2,
  Z = 3,
};

struct ShipMotionAxisBinding {
  ShipMotionAxis axis = ShipMotionAxis::None;
  float sign = 1.0f;
};

struct ShipControlConfig {
  float moveSpeed = 18.0f;
  float turnSpeed = 2.25f;
  float rollSpeed = 2.8f;
  float boostMultiplier = 1.8f;
  float brakeStrength = 22.0f;
  float stickDeadzone = 0.10f;
  float gyroSensitivity = 0.00008f;
  float gyroSmoothing = 0.12f;
  float motionInputDeadzone = 0.035f;
  float gyroCalibrationTime = 0.75f;
  float gyroAutoCalibrationTime = 0.65f;
  float gyroStillnessGyroDelta = 18.0f;
  float gyroStillnessAccelDelta = 120.0f;
  float gyroBiasLerpSpeed = 8.0f;
  float motionDodgeThreshold = 14500.0f;
  float autoFollowStrength = 0.42f;
  float autoFollowMaxDistance = 80.0f;
  float targetLockAngle = 28.0f;
  float vibrationIntensityScale = 0.55f;
  ShipMotionAxisBinding motionPitchAccel{ShipMotionAxis::Z, 1.0f};
  ShipMotionAxisBinding motionRollAccel{ShipMotionAxis::X, 1.0f};
  ShipMotionAxisBinding motionPitchGyro{ShipMotionAxis::X, 1.0f};
  ShipMotionAxisBinding motionYawGyro{ShipMotionAxis::Y, 1.0f};
  ShipMotionAxisBinding motionRollGyro{ShipMotionAxis::Z, 1.0f};
  bool adaptiveTriggerEnabled = true;
  bool motionControlEnabled = false;
};

inline bool ds5_demo_parse_bool(const std::string& value) {
  return value == "1" || value == "true" || value == "TRUE" || value == "on" || value == "ON";
}

inline ShipMotionAxisBinding ds5_demo_parse_motion_axis(std::string value) {
  ShipMotionAxisBinding binding{};
  for (char& c : value) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  if (!value.empty() && value[0] == '-') {
    binding.sign = -1.0f;
    value.erase(0, 1);
  } else if (!value.empty() && value[0] == '+') {
    value.erase(0, 1);
  }
  if (value == "x") binding.axis = ShipMotionAxis::X;
  else if (value == "y") binding.axis = ShipMotionAxis::Y;
  else if (value == "z") binding.axis = ShipMotionAxis::Z;
  else {
    binding.axis = ShipMotionAxis::None;
    binding.sign = 0.0f;
  }
  return binding;
}

inline void ds5_demo_apply_config_value(ShipControlConfig& config, const std::string& key, const std::string& value) {
  const float f = std::strtof(value.c_str(), nullptr);
  if (key == "moveSpeed") config.moveSpeed = f;
  else if (key == "turnSpeed") config.turnSpeed = f;
  else if (key == "rollSpeed") config.rollSpeed = f;
  else if (key == "boostMultiplier") config.boostMultiplier = f;
  else if (key == "brakeStrength") config.brakeStrength = f;
  else if (key == "stickDeadzone") config.stickDeadzone = f;
  else if (key == "gyroSensitivity") config.gyroSensitivity = f;
  else if (key == "gyroSmoothing") config.gyroSmoothing = f;
  else if (key == "motionInputDeadzone") config.motionInputDeadzone = f;
  else if (key == "gyroCalibrationTime") config.gyroCalibrationTime = f;
  else if (key == "gyroAutoCalibrationTime") config.gyroAutoCalibrationTime = f;
  else if (key == "gyroStillnessGyroDelta") config.gyroStillnessGyroDelta = f;
  else if (key == "gyroStillnessAccelDelta") config.gyroStillnessAccelDelta = f;
  else if (key == "gyroBiasLerpSpeed") config.gyroBiasLerpSpeed = f;
  else if (key == "motionDodgeThreshold") config.motionDodgeThreshold = f;
  else if (key == "autoFollowStrength") config.autoFollowStrength = f;
  else if (key == "autoFollowMaxDistance") config.autoFollowMaxDistance = f;
  else if (key == "targetLockAngle") config.targetLockAngle = f;
  else if (key == "vibrationIntensityScale") config.vibrationIntensityScale = f;
  else if (key == "motionPitchAccelAxis") config.motionPitchAccel = ds5_demo_parse_motion_axis(value);
  else if (key == "motionRollAccelAxis") config.motionRollAccel = ds5_demo_parse_motion_axis(value);
  else if (key == "motionPitchGyroAxis") config.motionPitchGyro = ds5_demo_parse_motion_axis(value);
  else if (key == "motionYawGyroAxis") config.motionYawGyro = ds5_demo_parse_motion_axis(value);
  else if (key == "motionRollGyroAxis") config.motionRollGyro = ds5_demo_parse_motion_axis(value);
  else if (key == "adaptiveTriggerEnabled") config.adaptiveTriggerEnabled = ds5_demo_parse_bool(value);
  else if (key == "motionControlEnabled") config.motionControlEnabled = ds5_demo_parse_bool(value);
}

inline ShipControlConfig ds5_demo_load_config(const char* path) {
  ShipControlConfig config{};
  std::ifstream file(path);
  if (!file) {
    return config;
  }

  std::string line;
  while (std::getline(file, line)) {
    const size_t comment = line.find('#');
    if (comment != std::string::npos) line.erase(comment);
    const size_t equals = line.find('=');
    if (equals == std::string::npos) continue;
    std::string key = line.substr(0, equals);
    std::string value = line.substr(equals + 1);
    auto trim = [](std::string& s) {
      const char* ws = " \t\r\n";
      const size_t begin = s.find_first_not_of(ws);
      const size_t end = s.find_last_not_of(ws);
      s = begin == std::string::npos ? "" : s.substr(begin, end - begin + 1);
    };
    trim(key);
    trim(value);
    ds5_demo_apply_config_value(config, key, value);
  }
  return config;
}

#endif
