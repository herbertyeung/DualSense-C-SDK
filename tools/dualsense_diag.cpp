#include <dualsense/dualsense.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

namespace {

void print_result(ds5_result result) {
  if (result != DS5_OK) {
    std::cerr << "error: " << ds5_get_last_error() << " (" << result << ")\n";
  }
}

void print_state_line(const ds5_state& state) {
  std::cout << "buttons=0x" << std::hex << std::setw(4) << std::setfill('0') << state.buttons
            << std::dec << std::setfill(' ')
            << " dpad=" << static_cast<int>(state.dpad)
            << " lx=" << static_cast<int>(state.left_stick_x)
            << " ly=" << static_cast<int>(state.left_stick_y)
            << " rx=" << static_cast<int>(state.right_stick_x)
            << " ry=" << static_cast<int>(state.right_stick_y)
            << " l2=" << static_cast<int>(state.left_trigger)
            << " r2=" << static_cast<int>(state.right_trigger)
            << " gyro=(" << state.gyro_x << "," << state.gyro_y << "," << state.gyro_z << ")"
            << " accel=(" << state.accel_x << "," << state.accel_y << "," << state.accel_z << ")"
            << " raw=";
  const uint32_t raw = state.raw_report_size < 32u ? state.raw_report_size : 32u;
  for (uint32_t i = 0; i < raw; ++i) {
    std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(state.raw_report[i]);
    if (i + 1u < raw) std::cout << ' ';
  }
  std::cout << std::dec << std::setfill(' ') << "\n";
}

void print_capabilities(uint32_t flags) {
  struct FlagName {
    uint32_t flag;
    const char* name;
  };
  const FlagName names[] = {
      {DS5_CAP_INPUT, "input"},
      {DS5_CAP_LIGHTBAR, "lightbar"},
      {DS5_CAP_PLAYER_LEDS, "player-leds"},
      {DS5_CAP_MIC_LED, "mic-led"},
      {DS5_CAP_CLASSIC_RUMBLE, "classic-rumble"},
      {DS5_CAP_HAPTICS, "haptics"},
      {DS5_CAP_ADAPTIVE_TRIGGERS, "adaptive-triggers"},
      {DS5_CAP_AUDIO_SPEAKER, "speaker"},
      {DS5_CAP_AUDIO_MICROPHONE, "microphone"},
      {DS5_CAP_HEADSET_JACK, "headset-jack"},
      {DS5_CAP_TOUCHPAD, "touchpad"},
      {DS5_CAP_IMU, "imu"},
      {DS5_CAP_RAW_REPORTS, "raw-reports"},
  };
  std::cout << "    capability names:";
  for (const auto& item : names) {
    if ((flags & item.flag) != 0u) {
      std::cout << ' ' << item.name;
    }
  }
  std::cout << "\n";
}

bool open_first_device(ds5_context* context, const std::vector<ds5_device_info>& devices, ds5_device** device) {
  if (devices.empty()) {
    std::cerr << "error: no DualSense device found\n";
    return false;
  }
  ds5_result result = ds5_open(context, &devices[0], device);
  if (result != DS5_OK) {
    print_result(result);
    return false;
  }
  return true;
}

std::vector<int16_t> make_tone(uint32_t sample_rate, uint32_t duration_ms) {
  const uint32_t frames = sample_rate * duration_ms / 1000u;
  std::vector<int16_t> pcm(frames * 2u);
  for (uint32_t i = 0; i < frames; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(sample_rate);
    const int16_t sample = static_cast<int16_t>(std::sin(t * 880.0 * 6.283185307179586) * 22000.0);
    pcm[i * 2u + 0u] = sample;
    pcm[i * 2u + 1u] = sample;
  }
  return pcm;
}

bool load_wav_pcm16(const char* path, std::vector<uint8_t>& pcm, ds5_audio_format& format) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  if (data.size() < 44u || std::memcmp(data.data(), "RIFF", 4) != 0 ||
      std::memcmp(data.data() + 8, "WAVE", 4) != 0) {
    return false;
  }

  uint16_t audio_format = 0;
  uint16_t channels = 0;
  uint32_t sample_rate = 0;
  uint16_t bits = 0;
  size_t cursor = 12u;
  size_t data_offset = 0u;
  uint32_t data_size = 0u;
  while (cursor + 8u <= data.size()) {
    const char* id = reinterpret_cast<const char*>(data.data() + cursor);
    uint32_t size = 0;
    std::memcpy(&size, data.data() + cursor + 4u, 4u);
    cursor += 8u;
    if (cursor + size > data.size()) {
      break;
    }
    if (std::memcmp(id, "fmt ", 4) == 0 && size >= 16u) {
      std::memcpy(&audio_format, data.data() + cursor, 2u);
      std::memcpy(&channels, data.data() + cursor + 2u, 2u);
      std::memcpy(&sample_rate, data.data() + cursor + 4u, 4u);
      std::memcpy(&bits, data.data() + cursor + 14u, 2u);
    } else if (std::memcmp(id, "data", 4) == 0) {
      data_offset = cursor;
      data_size = size;
    }
    cursor += size + (size & 1u);
  }

  if (audio_format != 1u || channels == 0u || sample_rate == 0u || bits != 16u ||
      data_offset == 0u || data_size == 0u || data_offset + data_size > data.size()) {
    return false;
  }

  pcm.assign(data.begin() + data_offset, data.begin() + data_offset + data_size);
  format.size = sizeof(format);
  format.version = DS5_STRUCT_VERSION;
  format.sample_rate = sample_rate;
  format.channels = channels;
  format.bits_per_sample = bits;
  return true;
}

void capture_callback(const void*, uint32_t bytes, const ds5_audio_format* format, void* user_data) {
  auto* total = static_cast<std::atomic<uint32_t>*>(user_data);
  total->fetch_add(bytes, std::memory_order_relaxed);
  if (format) {
    std::cout << "capture packet: " << bytes << " bytes, "
              << format->sample_rate << " Hz, channels=" << format->channels << "\n";
  }
}

}  // namespace

int main(int argc, char** argv) {
  ds5_context* context = nullptr;
  ds5_result result = ds5_init(&context);
  if (result != DS5_OK) {
    print_result(result);
    return 1;
  }

  uint32_t count = 0;
  result = ds5_enumerate(context, nullptr, 0, &count);
  if (result != DS5_OK && result != DS5_E_INSUFFICIENT_BUFFER) {
    print_result(result);
    ds5_shutdown(context);
    return 1;
  }

  std::vector<ds5_device_info> devices(count);
  for (auto& device : devices) {
    device.size = sizeof(device);
    device.version = DS5_STRUCT_VERSION;
  }
  result = ds5_enumerate(context, devices.data(), count, &count);
  if (result != DS5_OK) {
    print_result(result);
    ds5_shutdown(context);
    return 1;
  }

  std::cout << "DualSense devices: " << count << "\n";
  for (uint32_t i = 0; i < count; ++i) {
    const auto& d = devices[i];
    std::cout << "[" << i << "] vid=0x" << std::hex << d.vendor_id << " pid=0x" << d.product_id << std::dec
              << " transport=" << static_cast<int>(d.transport) << " product=" << d.product << "\n"
              << "    path=" << d.path << "\n"
              << "    capabilities=0x" << std::hex << d.capabilities.flags << std::dec << "\n";
    print_capabilities(d.capabilities.flags);
  }

  uint32_t audio_count = 0;
  result = ds5_audio_enumerate_endpoints(context, nullptr, 0, &audio_count);
  std::vector<ds5_audio_endpoint> endpoints(audio_count);
  for (auto& endpoint : endpoints) {
    endpoint.size = sizeof(endpoint);
    endpoint.version = DS5_STRUCT_VERSION;
  }
  if (audio_count > 0) {
    ds5_audio_enumerate_endpoints(context, endpoints.data(), audio_count, &audio_count);
  }
  std::cout << "DualSense audio endpoints: " << audio_count << "\n";
  for (uint32_t i = 0; i < audio_count; ++i) {
    std::cout << "[" << i << "] " << (endpoints[i].is_capture ? "capture" : "render")
              << " name=" << endpoints[i].name << "\n"
              << "    id=" << endpoints[i].id << "\n";
  }

  if (argc > 1 && std::string(argv[1]) == "--capabilities") {
    ds5_device* device = nullptr;
    if (open_first_device(context, devices, &device)) {
      ds5_capabilities capabilities{};
      capabilities.size = sizeof(capabilities);
      capabilities.version = DS5_STRUCT_VERSION;
      result = ds5_get_capabilities(device, &capabilities);
      if (result == DS5_OK) {
        std::cout << "Opened capabilities=0x" << std::hex << capabilities.flags << std::dec << "\n";
        print_capabilities(capabilities.flags);
      } else {
        print_result(result);
      }
      ds5_close(device);
    }
  }

  if (argc > 1 && std::string(argv[1]) == "--test") {
    ds5_device* device = nullptr;
    if (open_first_device(context, devices, &device)) {
      std::cout << "Running output coverage test, then resetting outputs.\n";
      print_result(ds5_set_lightbar(device, 0, 64, 255));
      print_result(ds5_set_player_leds(device, 0x15));
      print_result(ds5_set_mic_led(device, DS5_MIC_LED_ON));
      print_result(ds5_set_rumble(device, 80, 80));
      print_result(ds5_set_haptic_pattern(device, 48, 96, 120));

      ds5_trigger_effect effects[5]{};
      effects[0].mode = DS5_TRIGGER_EFFECT_CONSTANT_RESISTANCE;
      effects[0].start_position = 20;
      effects[0].force = 180;
      effects[1].mode = DS5_TRIGGER_EFFECT_SECTION_RESISTANCE;
      effects[1].start_position = 10;
      effects[1].end_position = 28;
      effects[1].force = 150;
      effects[2].mode = DS5_TRIGGER_EFFECT_WEAPON;
      effects[2].start_position = 2;
      effects[2].end_position = 8;
      effects[2].force = 220;
      effects[3].mode = DS5_TRIGGER_EFFECT_VIBRATION;
      effects[3].start_position = 12;
      effects[3].end_position = 28;
      effects[3].force = 190;
      effects[3].frequency = 30;
      effects[4].mode = DS5_TRIGGER_EFFECT_OFF;

      for (const auto& effect : effects) {
        print_result(ds5_set_trigger_effect(device, 1, &effect));
        print_result(ds5_set_trigger_effect(device, 0, &effect));
        std::this_thread::sleep_for(std::chrono::milliseconds(220));
      }

      ds5_trigger_effect off{};
      print_result(ds5_set_trigger_effect(device, 1, &off));
      print_result(ds5_set_trigger_effect(device, 0, &off));
      print_result(ds5_set_rumble(device, 0, 0));
      print_result(ds5_set_mic_led(device, DS5_MIC_LED_OFF));
      ds5_close(device);
    }
  }

  if (argc > 1 && std::string(argv[1]) == "--raw-output-reset") {
    ds5_device* device = nullptr;
    if (open_first_device(context, devices, &device)) {
      uint8_t report[48]{};
      report[0] = 0x02;
      report[1] = 0xff;
      report[2] = 0xf7;
      print_result(ds5_send_raw_output_report(device, report, sizeof(report)));
      ds5_close(device);
    }
  }

  if (argc > 1 && std::string(argv[1]) == "--poll") {
    uint32_t frames = 180;
    if (argc > 2) {
      frames = static_cast<uint32_t>(std::max(1, std::stoi(argv[2])));
    }
    ds5_device* device = nullptr;
    if (open_first_device(context, devices, &device)) {
      std::cout << "Polling first DualSense for " << frames
                << " reports. Move/tilt the controller and press Triangle.\n";
      for (uint32_t i = 0; i < frames; ++i) {
        ds5_state state{};
        state.size = sizeof(state);
        state.version = DS5_STRUCT_VERSION;
        result = ds5_poll_state(device, &state);
        if (result != DS5_OK) {
          print_result(result);
          break;
        }
        print_state_line(state);
      }
      ds5_close(device);
    }
  }

  if (argc > 1 && std::string(argv[1]) == "--tone") {
    uint32_t duration_ms = 1500;
    if (argc > 2) {
      duration_ms = static_cast<uint32_t>(std::max(100, std::stoi(argv[2])));
    }
    for (const auto& endpoint : endpoints) {
      if (!endpoint.is_capture) {
        std::cout << "Playing " << duration_ms << "ms tone on " << endpoint.name << "\n";
        ds5_audio_format format{};
        format.size = sizeof(format);
        format.version = DS5_STRUCT_VERSION;
        format.sample_rate = 48000;
        format.channels = 2;
        format.bits_per_sample = 16;
        auto tone = make_tone(format.sample_rate, duration_ms);
        result = ds5_audio_play_pcm(context, endpoint.id, tone.data(), static_cast<uint32_t>(tone.size() * sizeof(int16_t)), &format);
        print_result(result);
        if (result == DS5_OK) {
          std::cout << "tone playback completed\n";
        }
        break;
      }
    }
  }

  if (argc > 2 && std::string(argv[1]) == "--play-wav") {
    std::vector<uint8_t> pcm;
    ds5_audio_format format{};
    if (!load_wav_pcm16(argv[2], pcm, format)) {
      std::cerr << "error: --play-wav requires a PCM 16-bit WAV file\n";
      ds5_shutdown(context);
      return 1;
    }
    for (const auto& endpoint : endpoints) {
      if (!endpoint.is_capture) {
        const uint32_t frame_bytes = static_cast<uint32_t>(format.channels * (format.bits_per_sample / 8u));
        const uint32_t duration_ms = frame_bytes > 0u && format.sample_rate > 0u
                                         ? static_cast<uint32_t>((static_cast<uint64_t>(pcm.size()) * 1000u) /
                                                                 (static_cast<uint64_t>(frame_bytes) * format.sample_rate))
                                         : 0u;
        std::cout << "Playing WAV on " << endpoint.name << " (" << format.sample_rate << " Hz, "
                  << format.channels << " channels, " << duration_ms << "ms)\n";
        result = ds5_audio_play_pcm(context, endpoint.id, pcm.data(), static_cast<uint32_t>(pcm.size()), &format);
        print_result(result);
        if (result == DS5_OK) {
          std::cout << "wav playback completed\n";
        }
        break;
      }
    }
  }

  if (argc > 1 && std::string(argv[1]) == "--capture") {
    uint32_t duration_ms = 1000;
    if (argc > 2) {
      duration_ms = static_cast<uint32_t>(std::max(1, std::stoi(argv[2])));
    }
    for (const auto& endpoint : endpoints) {
      if (endpoint.is_capture) {
        ds5_audio_format format{};
        format.size = sizeof(format);
        format.version = DS5_STRUCT_VERSION;
        format.sample_rate = 48000;
        format.channels = 1;
        format.bits_per_sample = 16;
        std::atomic<uint32_t> captured_bytes{0};
        ds5_audio_capture* capture = nullptr;
        result = ds5_audio_capture_start(context, endpoint.id, &format, capture_callback, &captured_bytes, &capture);
        if (result == DS5_OK) {
          std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
          ds5_audio_capture_stop(capture);
          std::cout << "capture total bytes: " << captured_bytes.load() << "\n";
        } else {
          print_result(result);
        }
        break;
      }
    }
  }

  ds5_shutdown(context);
  return 0;
}
