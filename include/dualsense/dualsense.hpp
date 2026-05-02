#ifndef DUALSENSE_DUALSENSE_HPP
#define DUALSENSE_DUALSENSE_HPP

#include <stdexcept>
#include <string>
#include <vector>

#include "dualsense.h"

namespace DualSense {

class Error : public std::runtime_error {
 public:
  explicit Error(const std::string& message) : std::runtime_error(message) {}
};

inline void throwIfFailed(ds5_result result) {
  if (result != DS5_OK) {
    throw Error(ds5_get_last_error());
  }
}

class Context {
 public:
  Context() {
    throwIfFailed(ds5_init(&context_));
  }

  ~Context() {
    ds5_shutdown(context_);
  }

  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  Context(Context&& other) noexcept : context_(other.context_) {
    other.context_ = nullptr;
  }

  Context& operator=(Context&& other) noexcept {
    if (this != &other) {
      ds5_shutdown(context_);
      context_ = other.context_;
      other.context_ = nullptr;
    }
    return *this;
  }

  ds5_context* native() const { return context_; }

  std::vector<ds5_device_info> enumerate() const {
    uint32_t count = 0;
    ds5_result result = ds5_enumerate(context_, nullptr, 0, &count);
    if (result != DS5_OK && result != DS5_E_INSUFFICIENT_BUFFER) {
      throwIfFailed(result);
    }
    std::vector<ds5_device_info> devices(count);
    for (auto& device : devices) {
      device.size = sizeof(device);
      device.version = DS5_STRUCT_VERSION;
    }
    throwIfFailed(ds5_enumerate(context_, devices.data(), count, &count));
    devices.resize(count);
    return devices;
  }

 private:
  ds5_context* context_{nullptr};
};

class Controller {
 public:
  Controller() = default;

  Controller(ds5_context* context, const ds5_device_info& info) {
    throwIfFailed(ds5_open(context, &info, &device_));
  }

  ~Controller() {
    ds5_close(device_);
  }

  Controller(const Controller&) = delete;
  Controller& operator=(const Controller&) = delete;

  Controller(Controller&& other) noexcept : device_(other.device_) {
    other.device_ = nullptr;
  }

  Controller& operator=(Controller&& other) noexcept {
    if (this != &other) {
      ds5_close(device_);
      device_ = other.device_;
      other.device_ = nullptr;
    }
    return *this;
  }

  static Controller openFirstUsb(Context& context) {
    for (const auto& info : context.enumerate()) {
      if (info.transport == DS5_TRANSPORT_USB) {
        return Controller(context.native(), info);
      }
    }
    throw Error("No USB DualSense controller found");
  }

  ds5_state state() const {
    ds5_state state{};
    state.size = sizeof(state);
    state.version = DS5_STRUCT_VERSION;
    throwIfFailed(ds5_poll_state(device_, &state));
    return state;
  }

  ds5_capabilities capabilities() const {
    ds5_capabilities capabilities{};
    capabilities.size = sizeof(capabilities);
    capabilities.version = DS5_STRUCT_VERSION;
    throwIfFailed(ds5_get_capabilities(device_, &capabilities));
    return capabilities;
  }

  class Haptics {
   public:
    explicit Haptics(ds5_device* device) : device_(device) {}
    void rumble(uint8_t left, uint8_t right) const { throwIfFailed(ds5_set_rumble(device_, left, right)); }
    void pattern(uint8_t left, uint8_t right, uint32_t duration_ms) const { throwIfFailed(ds5_set_haptic_pattern(device_, left, right, duration_ms)); }
   private:
    ds5_device* device_;
  };

  class Triggers {
   public:
    explicit Triggers(ds5_device* device) : device_(device) {}
    void setEffect(bool left, const ds5_trigger_effect& effect) const {
      throwIfFailed(ds5_set_trigger_effect(device_, left ? 1u : 0u, &effect));
    }
    void setResistance(bool left, uint8_t start, uint8_t force) const {
      ds5_trigger_effect effect{};
      effect.mode = DS5_TRIGGER_EFFECT_CONSTANT_RESISTANCE;
      effect.start_position = start;
      effect.force = force;
      setEffect(left, effect);
    }
    void off(bool left) const {
      ds5_trigger_effect effect{};
      setEffect(left, effect);
    }
    void off() const {
      off(true);
      off(false);
    }
   private:
    ds5_device* device_;
  };

  Haptics haptics() const { return Haptics(device_); }
  Triggers triggers() const { return Triggers(device_); }
  void setPlayerLeds(uint8_t mask) const {
    throwIfFailed(ds5_set_player_leds(device_, mask));
  }
  void setMicLed(ds5_mic_led mode) const {
    throwIfFailed(ds5_set_mic_led(device_, mode));
  }
  void setLightbar(uint8_t r, uint8_t g, uint8_t b) const {
    throwIfFailed(ds5_set_lightbar(device_, r, g, b));
  }
  void sendRawOutputReport(const void* bytes, uint32_t size) const {
    throwIfFailed(ds5_send_raw_output_report(device_, bytes, size));
  }
  void resetFeedback() const {
    ds5_trigger_effect off{};
    throwIfFailed(ds5_set_trigger_effect(device_, 1u, &off));
    throwIfFailed(ds5_set_trigger_effect(device_, 0u, &off));
    throwIfFailed(ds5_set_rumble(device_, 0, 0));
    throwIfFailed(ds5_set_mic_led(device_, DS5_MIC_LED_OFF));
  }

 private:
  ds5_device* device_{nullptr};
};

}  // namespace DualSense

#endif
