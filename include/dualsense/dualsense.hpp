/*
  File: dualsense.hpp
  Author: Herbert Yeung
  Purpose: C++ RAII wrapper for the DualSense C SDK.
*/

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

struct Version {
  uint32_t major;
  uint32_t minor;
  uint32_t patch;
};

inline Version runtimeVersion() {
  Version version{};
  ds5_get_version(&version.major, &version.minor, &version.patch);
  return version;
}

inline std::string runtimeVersionString() {
  return ds5_get_version_string();
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
      ds5_device_info_init(&device);
    }
    throwIfFailed(ds5_enumerate(context_, devices.data(), count, &count));
    devices.resize(count);
    return devices;
  }

  std::vector<ds5_audio_endpoint> audioEndpoints() const {
    uint32_t count = 0;
    ds5_result result = ds5_audio_enumerate_endpoints(context_, nullptr, 0, &count);
    if (result != DS5_OK && result != DS5_E_INSUFFICIENT_BUFFER) {
      throwIfFailed(result);
    }
    std::vector<ds5_audio_endpoint> endpoints(count);
    for (auto& endpoint : endpoints) {
      ds5_audio_endpoint_init(&endpoint);
    }
    throwIfFailed(ds5_audio_enumerate_endpoints(context_, endpoints.data(), count, &count));
    endpoints.resize(count);
    return endpoints;
  }

  void playPcm(const std::string& endpointId, const void* pcm, uint32_t bytes, const ds5_audio_format& format) const {
    throwIfFailed(ds5_audio_play_pcm(context_, endpointId.empty() ? nullptr : endpointId.c_str(), pcm, bytes, &format));
  }

 private:
  ds5_context* context_{nullptr};
};

class AudioCapture {
 public:
  AudioCapture() = default;

  AudioCapture(Context& context,
               const std::string& endpointId,
               const ds5_audio_format& format,
               ds5_audio_capture_callback callback,
               void* userData) {
    throwIfFailed(ds5_audio_capture_start(context.native(),
                                          endpointId.empty() ? nullptr : endpointId.c_str(),
                                          &format,
                                          callback,
                                          userData,
                                          &capture_));
  }

  ~AudioCapture() {
    ds5_audio_capture_stop(capture_);
  }

  AudioCapture(const AudioCapture&) = delete;
  AudioCapture& operator=(const AudioCapture&) = delete;

  AudioCapture(AudioCapture&& other) noexcept : capture_(other.capture_) {
    other.capture_ = nullptr;
  }

  AudioCapture& operator=(AudioCapture&& other) noexcept {
    if (this != &other) {
      ds5_audio_capture_stop(capture_);
      capture_ = other.capture_;
      other.capture_ = nullptr;
    }
    return *this;
  }

  ds5_audio_capture* native() const { return capture_; }

 private:
  ds5_audio_capture* capture_{nullptr};
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
    ds5_state_init(&state);
    throwIfFailed(ds5_poll_state(device_, &state));
    return state;
  }

  ds5_state state(uint32_t timeoutMs) const {
    ds5_state state{};
    ds5_state_init(&state);
    throwIfFailed(ds5_poll_state_timeout(device_, timeoutMs, &state));
    return state;
  }

  bool tryState(ds5_state& state) const {
    ds5_state_init(&state);
    ds5_result result = ds5_try_poll_state(device_, &state);
    if (result == DS5_E_TIMEOUT) {
      return false;
    }
    throwIfFailed(result);
    return true;
  }

  ds5_capabilities capabilities() const {
    ds5_capabilities capabilities{};
    ds5_capabilities_init(&capabilities);
    throwIfFailed(ds5_get_capabilities(device_, &capabilities));
    return capabilities;
  }

  bool hasCapability(ds5_capability_flags capability) const {
    return (capabilities().flags & static_cast<uint32_t>(capability)) != 0u;
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
    static ds5_trigger_effect constantResistance(uint8_t start, uint8_t force) {
      ds5_trigger_effect effect{};
      ds5_trigger_effect_constant_resistance(&effect, start, force);
      return effect;
    }
    static ds5_trigger_effect sectionResistance(uint8_t start, uint8_t end, uint8_t force) {
      ds5_trigger_effect effect{};
      ds5_trigger_effect_section_resistance(&effect, start, end, force);
      return effect;
    }
    static ds5_trigger_effect weapon(uint8_t start, uint8_t end, uint8_t force) {
      ds5_trigger_effect effect{};
      ds5_trigger_effect_weapon(&effect, start, end, force);
      return effect;
    }
    static ds5_trigger_effect vibration(uint8_t start, uint8_t end, uint8_t force, uint8_t frequency) {
      ds5_trigger_effect effect{};
      ds5_trigger_effect_vibration(&effect, start, end, force, frequency);
      return effect;
    }
    void setResistance(bool left, uint8_t start, uint8_t force) const {
      setEffect(left, constantResistance(start, force));
    }
    void off(bool left) const {
      ds5_trigger_effect effect{};
      ds5_trigger_effect_off(&effect);
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
    throwIfFailed(ds5_reset_feedback(device_));
  }

 private:
  ds5_device* device_{nullptr};
};

}  // namespace DualSense

#endif
