#ifndef DS5_SHIP_FEEDBACK_H
#define DS5_SHIP_FEEDBACK_H

#include <algorithm>
#include <cstdint>

#include <dualsense/dualsense.h>

#include "ship_controls.h"
#include "ship_systems.h"

struct ShipFeedback {
  ds5_trigger_effect right_trigger{};
  ds5_trigger_effect left_trigger{};
  uint8_t left_rumble = 0;
  uint8_t right_rumble = 0;
  uint8_t light_r = 0;
  uint8_t light_g = 64;
  uint8_t light_b = 255;
  const char* event_name = "engine";
};

enum class ShipTriggerMode {
  Flight = 0,
  Light = 1,
  Heavy = 2,
  Charge = 3,
  Off = 4,
};

inline ShipTriggerMode ds5_demo_next_trigger_mode(ShipTriggerMode mode) {
  const int next = (static_cast<int>(mode) + 1) % 5;
  return static_cast<ShipTriggerMode>(next);
}

inline const char* ds5_demo_trigger_mode_name(ShipTriggerMode mode) {
  switch (mode) {
    case ShipTriggerMode::Flight: return "Flight";
    case ShipTriggerMode::Light: return "Light";
    case ShipTriggerMode::Heavy: return "Heavy";
    case ShipTriggerMode::Charge: return "Charge";
    case ShipTriggerMode::Off: return "Off";
    default: return "Unknown";
  }
}

inline ds5_trigger_effect ds5_demo_r2_auto_fire_trigger_effect(ShipFeedbackZoneKind zone) {
  ds5_trigger_effect effect{};
  effect.mode = DS5_TRIGGER_EFFECT_VIBRATION;
  effect.start_position = 22;
  effect.end_position = 210;
  effect.force = 220;
  effect.frequency = 34;
  if (zone == ShipFeedbackZoneKind::Pulse) {
    effect.force = 175;
    effect.frequency = 18;
  } else if (zone == ShipFeedbackZoneKind::Heavy) {
    effect.start_position = 14;
    effect.force = 255;
    effect.frequency = 24;
  } else if (zone == ShipFeedbackZoneKind::Rapid) {
    effect.start_position = 28;
    effect.force = 205;
    effect.frequency = 54;
  } else if (zone == ShipFeedbackZoneKind::Sticky) {
    effect.mode = DS5_TRIGGER_EFFECT_WEAPON;
    effect.start_position = 30;
    effect.end_position = 128;
    effect.force = 230;
    effect.frequency = 12;
  }
  return effect;
}

inline ds5_trigger_effect ds5_demo_r2_auto_fire_trigger_effect() {
  return ds5_demo_r2_auto_fire_trigger_effect(ShipFeedbackZoneKind::None);
}

inline void ds5_demo_apply_zone_feedback(ShipFeedback& feedback, ShipFeedbackZoneKind zone) {
  if (zone == ShipFeedbackZoneKind::None) return;
  if (zone == ShipFeedbackZoneKind::Pulse) {
    feedback.left_rumble = std::max<uint8_t>(feedback.left_rumble, 28);
    feedback.right_rumble = std::max<uint8_t>(feedback.right_rumble, 42);
    feedback.light_r = 45;
    feedback.light_g = 140;
    feedback.light_b = 255;
    feedback.event_name = "zone pulse";
  } else if (zone == ShipFeedbackZoneKind::Heavy) {
    feedback.left_rumble = std::max<uint8_t>(feedback.left_rumble, 75);
    feedback.right_rumble = std::max<uint8_t>(feedback.right_rumble, 95);
    feedback.right_trigger.mode = DS5_TRIGGER_EFFECT_CONSTANT_RESISTANCE;
    feedback.right_trigger.start_position = 16;
    feedback.right_trigger.force = std::max<uint8_t>(feedback.right_trigger.force, 190);
    feedback.light_r = 255;
    feedback.light_g = 45;
    feedback.light_b = 28;
    feedback.event_name = "zone heavy";
  } else if (zone == ShipFeedbackZoneKind::Rapid) {
    feedback.left_rumble = std::max<uint8_t>(feedback.left_rumble, 22);
    feedback.right_rumble = std::max<uint8_t>(feedback.right_rumble, 65);
    feedback.light_r = 35;
    feedback.light_g = 255;
    feedback.light_b = 115;
    feedback.event_name = "zone rapid";
  } else if (zone == ShipFeedbackZoneKind::Sticky) {
    feedback.left_rumble = std::max<uint8_t>(feedback.left_rumble, 55);
    feedback.right_rumble = std::max<uint8_t>(feedback.right_rumble, 30);
    feedback.right_trigger.mode = DS5_TRIGGER_EFFECT_SECTION_RESISTANCE;
    feedback.right_trigger.start_position = 42;
    feedback.right_trigger.end_position = 160;
    feedback.right_trigger.force = std::max<uint8_t>(feedback.right_trigger.force, 165);
    feedback.light_r = 180;
    feedback.light_g = 70;
    feedback.light_b = 255;
    feedback.event_name = "zone sticky";
  }
}

inline ShipFeedback ds5_demo_feedback_from_ship(const ShipPose& pose, const ShipInput& input,
                                                ShipTriggerMode trigger_mode = ShipTriggerMode::Flight,
                                                bool weapon_charging = false, bool low_health = false) {
  ShipFeedback feedback{};

  const float speed_factor = ds5_demo_clamp(pose.speed / 32.0f, 0.0f, 1.0f);
  const float thrust_factor = ds5_demo_clamp(input.throttle, 0.0f, 1.0f);
  const float brake_factor = ds5_demo_clamp(input.brake, 0.0f, 1.0f);
  const bool boost = thrust_factor > 0.9f;

  if (trigger_mode == ShipTriggerMode::Off) {
    feedback.right_trigger.mode = DS5_TRIGGER_EFFECT_OFF;
    feedback.left_trigger.mode = DS5_TRIGGER_EFFECT_OFF;
    feedback.event_name = "trigger off";
  } else if (trigger_mode == ShipTriggerMode::Charge || weapon_charging) {
    feedback.right_trigger.mode = DS5_TRIGGER_EFFECT_SECTION_RESISTANCE;
    feedback.right_trigger.start_position = 42;
    feedback.right_trigger.end_position = 160;
    feedback.right_trigger.force = static_cast<uint8_t>(70.0f + 120.0f * thrust_factor);
    feedback.event_name = "weapon charge";
  } else {
    feedback.right_trigger.mode = DS5_TRIGGER_EFFECT_CONSTANT_RESISTANCE;
    if (trigger_mode == ShipTriggerMode::Light) {
      feedback.right_trigger.start_position = 34;
      feedback.right_trigger.force = static_cast<uint8_t>(8.0f + 45.0f * thrust_factor + 70.0f * speed_factor);
      feedback.event_name = "trigger light";
    } else if (trigger_mode == ShipTriggerMode::Heavy) {
      feedback.right_trigger.start_position = 22;
      feedback.right_trigger.force = static_cast<uint8_t>(45.0f + 65.0f * thrust_factor + 135.0f * speed_factor);
      feedback.event_name = "trigger heavy";
    } else if (boost) {
      feedback.right_trigger.start_position = 18;
      feedback.right_trigger.force = static_cast<uint8_t>(150.0f + 70.0f * speed_factor);
      feedback.event_name = "boost";
    } else {
      feedback.right_trigger.start_position = 30;
      feedback.right_trigger.force = static_cast<uint8_t>(12.0f + 68.0f * thrust_factor + 80.0f * speed_factor);
    }
  }

  if (trigger_mode != ShipTriggerMode::Off) {
    feedback.left_trigger.mode = DS5_TRIGGER_EFFECT_CONSTANT_RESISTANCE;
    feedback.left_trigger.start_position = trigger_mode == ShipTriggerMode::Heavy ? 12 : 18;
    feedback.left_trigger.force = static_cast<uint8_t>((trigger_mode == ShipTriggerMode::Heavy ? 28.0f : 12.0f) + 135.0f * brake_factor);
  }

  feedback.left_rumble = static_cast<uint8_t>(35.0f * brake_factor);
  feedback.right_rumble = static_cast<uint8_t>((boost ? 42.0f : 18.0f) * thrust_factor + 28.0f * speed_factor);
  if (low_health) {
    feedback.light_r = 255;
    feedback.light_g = 32;
    feedback.light_b = 16;
    feedback.event_name = "low health";
  } else if (trigger_mode == ShipTriggerMode::Charge || weapon_charging) {
    feedback.light_r = 255;
    feedback.light_g = 160;
    feedback.light_b = 20;
  } else if (boost) {
    feedback.light_r = 70;
    feedback.light_g = 190;
    feedback.light_b = 255;
  } else if (brake_factor > 0.2f) {
    feedback.light_r = 255;
    feedback.light_g = 70;
    feedback.light_b = 30;
    feedback.event_name = "brake";
  } else {
    feedback.light_r = static_cast<uint8_t>(20.0f + 80.0f * speed_factor);
    feedback.light_g = static_cast<uint8_t>(80.0f + 120.0f * speed_factor);
    feedback.light_b = 255;
  }
  return feedback;
}

#endif
