#include "core.h"

#include <audioclient.h>
#include <propkey.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <propsys.h>

#include <atomic>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <cmath>

struct ds5_audio_capture {
  std::atomic<bool> running{false};
  std::thread worker;
};

namespace {

class ComInit {
 public:
  ComInit() : hr_(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}
  ~ComInit() {
    if (SUCCEEDED(hr_)) {
      CoUninitialize();
    }
  }
  HRESULT hr() const { return hr_; }
 private:
  HRESULT hr_;
};

template <typename T>
class ComPtr {
 public:
  ~ComPtr() { reset(); }
  T** put() {
    reset();
    return &ptr_;
  }
  T* get() const { return ptr_; }
  T* operator->() const { return ptr_; }
  T** operator&() { return put(); }
  void reset() {
    if (ptr_) {
      ptr_->Release();
      ptr_ = nullptr;
    }
  }
 private:
  T* ptr_ = nullptr;
};

void copy_string(char* target, size_t target_size, const std::string& source) {
  if (!target || target_size == 0u) {
    return;
  }
  std::memset(target, 0, target_size);
  const size_t count = source.size() < target_size ? source.size() : target_size - 1u;
  std::memcpy(target, source.data(), count);
}

bool looks_like_dualsense_audio(const std::string& name) {
  std::string lowered = name;
  for (char& c : lowered) {
    c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  }
  return lowered.find("dualsense") != std::string::npos ||
         lowered.find("wireless controller") != std::string::npos ||
         lowered.find("ps5") != std::string::npos;
}

std::string endpoint_name(IMMDevice* device) {
  ComPtr<IPropertyStore> store;
  if (FAILED(device->OpenPropertyStore(STGM_READ, store.put()))) {
    return {};
  }

  PROPVARIANT value;
  PropVariantInit(&value);
  std::string result;
  if (SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &value)) && value.vt == VT_LPWSTR) {
    result = ds5_wide_to_utf8(value.pwszVal);
  }
  PropVariantClear(&value);
  return result;
}

std::string endpoint_id(IMMDevice* device) {
  LPWSTR id = nullptr;
  std::string result;
  if (SUCCEEDED(device->GetId(&id)) && id) {
    result = ds5_wide_to_utf8(id);
  }
  CoTaskMemFree(id);
  return result;
}

ds5_result create_enumerator(ComPtr<IMMDeviceEnumerator>& enumerator) {
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                reinterpret_cast<void**>(enumerator.put()));
  if (FAILED(hr)) {
    ds5_set_last_error_message("Failed to create MMDeviceEnumerator");
    return DS5_E_AUDIO;
  }
  return DS5_OK;
}

ds5_result get_device_by_id_or_default(IMMDeviceEnumerator* enumerator, const char* endpoint_id_value, EDataFlow flow, ComPtr<IMMDevice>& device) {
  if (endpoint_id_value && endpoint_id_value[0] != '\0') {
    const std::wstring wide_id = ds5_utf8_to_wide(endpoint_id_value);
    if (FAILED(enumerator->GetDevice(wide_id.c_str(), device.put()))) {
      ds5_set_last_error_message("Failed to open requested audio endpoint");
      return DS5_E_AUDIO;
    }
    return DS5_OK;
  }

  if (FAILED(enumerator->GetDefaultAudioEndpoint(flow, eConsole, device.put()))) {
    ds5_set_last_error_message("Failed to open default audio endpoint");
    return DS5_E_AUDIO;
  }
  return DS5_OK;
}

WAVEFORMATEX make_wave_format(const ds5_audio_format* format) {
  WAVEFORMATEX wave{};
  wave.wFormatTag = WAVE_FORMAT_PCM;
  wave.nChannels = format && format->channels ? format->channels : 2;
  wave.nSamplesPerSec = format && format->sample_rate ? format->sample_rate : 48000;
  wave.wBitsPerSample = format && format->bits_per_sample ? format->bits_per_sample : 16;
  wave.nBlockAlign = static_cast<WORD>((wave.nChannels * wave.wBitsPerSample) / 8u);
  wave.nAvgBytesPerSec = wave.nSamplesPerSec * wave.nBlockAlign;
  return wave;
}

bool is_float_format(const WAVEFORMATEX* wave) {
  if (wave->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
    return true;
  }
  if (wave->wFormatTag == WAVE_FORMAT_EXTENSIBLE && wave->cbSize >= 22u) {
    const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wave);
    return IsEqualGUID(extensible->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) != FALSE;
  }
  return false;
}

bool is_pcm_format(const WAVEFORMATEX* wave) {
  if (wave->wFormatTag == WAVE_FORMAT_PCM) {
    return true;
  }
  if (wave->wFormatTag == WAVE_FORMAT_EXTENSIBLE && wave->cbSize >= 22u) {
    const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wave);
    return IsEqualGUID(extensible->SubFormat, KSDATAFORMAT_SUBTYPE_PCM) != FALSE;
  }
  return false;
}

std::vector<uint8_t> convert_s16_pcm_to_mix(const void* pcm, uint32_t bytes, const ds5_audio_format* input_format, const WAVEFORMATEX* mix_format) {
  if (!pcm || !input_format || !mix_format || input_format->bits_per_sample != 16u ||
      input_format->sample_rate != mix_format->nSamplesPerSec || input_format->channels == 0u ||
      mix_format->nChannels == 0u) {
    return {};
  }

  const auto* source = static_cast<const int16_t*>(pcm);
  const uint32_t input_frame_bytes = input_format->channels * sizeof(int16_t);
  const uint32_t frames = bytes / input_frame_bytes;
  std::vector<uint8_t> converted(static_cast<size_t>(frames) * mix_format->nBlockAlign);

  if (is_float_format(mix_format) && mix_format->wBitsPerSample == 32u) {
    auto* destination = reinterpret_cast<float*>(converted.data());
    for (uint32_t frame = 0; frame < frames; ++frame) {
      for (uint16_t channel = 0; channel < mix_format->nChannels; ++channel) {
        const uint16_t source_channel = std::min<uint16_t>(channel, input_format->channels - 1u);
        destination[frame * mix_format->nChannels + channel] =
            static_cast<float>(source[frame * input_format->channels + source_channel]) / 32768.0f;
      }
    }
    return converted;
  }

  if (is_pcm_format(mix_format) && mix_format->wBitsPerSample == 16u) {
    auto* destination = reinterpret_cast<int16_t*>(converted.data());
    for (uint32_t frame = 0; frame < frames; ++frame) {
      for (uint16_t channel = 0; channel < mix_format->nChannels; ++channel) {
        const uint16_t source_channel = std::min<uint16_t>(channel, input_format->channels - 1u);
        destination[frame * mix_format->nChannels + channel] =
            source[frame * input_format->channels + source_channel];
      }
    }
    return converted;
  }

  return {};
}

}  // namespace

ds5_result ds5_audio_enumerate_endpoints(ds5_context* context, ds5_audio_endpoint* endpoints, uint32_t capacity, uint32_t* count) {
  (void)context;
  if (!count) {
    ds5_set_last_error_message("ds5_audio_enumerate_endpoints requires count");
    return DS5_E_INVALID_ARGUMENT;
  }

  ComInit com;
  if (FAILED(com.hr())) {
    ds5_set_last_error_message("Failed to initialize COM for audio enumeration");
    return DS5_E_AUDIO;
  }

  ComPtr<IMMDeviceEnumerator> enumerator;
  ds5_result result = create_enumerator(enumerator);
  if (result != DS5_OK) {
    return result;
  }

  std::vector<ds5_audio_endpoint> found;
  for (EDataFlow flow : {eRender, eCapture}) {
    ComPtr<IMMDeviceCollection> collection;
    if (FAILED(enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, collection.put()))) {
      continue;
    }

    UINT device_count = 0;
    collection->GetCount(&device_count);
    for (UINT i = 0; i < device_count; ++i) {
      ComPtr<IMMDevice> device;
      if (FAILED(collection->Item(i, device.put()))) {
        continue;
      }
      const std::string name = endpoint_name(device.get());
      if (!looks_like_dualsense_audio(name)) {
        continue;
      }

      ds5_audio_endpoint endpoint{};
      ds5_audio_endpoint_init(&endpoint);
      endpoint.is_capture = flow == eCapture ? 1u : 0u;
      copy_string(endpoint.id, sizeof(endpoint.id), endpoint_id(device.get()));
      copy_string(endpoint.name, sizeof(endpoint.name), name);
      found.push_back(endpoint);
    }
  }

  *count = static_cast<uint32_t>(found.size());
  if (!endpoints || capacity < found.size()) {
    return found.empty() ? DS5_OK : DS5_E_INSUFFICIENT_BUFFER;
  }
  for (size_t i = 0; i < found.size(); ++i) {
    if (!ds5_validate_struct(endpoints[i].size, endpoints[i].version, sizeof(ds5_audio_endpoint))) {
      ds5_set_last_error_message("ds5_audio_enumerate_endpoints received an uninitialized endpoint entry");
      return DS5_E_INVALID_ARGUMENT;
    }
    endpoints[i] = found[i];
  }
  return DS5_OK;
}

ds5_result ds5_audio_play_pcm(ds5_context* context, const char* endpoint_id_value, const void* pcm, uint32_t bytes, const ds5_audio_format* format) {
  (void)context;
  if (!pcm || bytes == 0u) {
    ds5_set_last_error_message("ds5_audio_play_pcm requires PCM data");
    return DS5_E_INVALID_ARGUMENT;
  }
  if (format && !ds5_validate_struct(format->size, format->version, sizeof(ds5_audio_format))) {
    ds5_set_last_error_message("ds5_audio_play_pcm received an uninitialized audio format");
    return DS5_E_INVALID_ARGUMENT;
  }

  ComInit com;
  if (FAILED(com.hr())) {
    ds5_set_last_error_message("Failed to initialize COM for audio playback");
    return DS5_E_AUDIO;
  }

  ComPtr<IMMDeviceEnumerator> enumerator;
  ds5_result result = create_enumerator(enumerator);
  if (result != DS5_OK) return result;

  ComPtr<IMMDevice> device;
  result = get_device_by_id_or_default(enumerator.get(), endpoint_id_value, eRender, device);
  if (result != DS5_OK) return result;

  ComPtr<IAudioClient> audio_client;
  if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(audio_client.put())))) {
    ds5_set_last_error_message("Failed to activate IAudioClient");
    return DS5_E_AUDIO;
  }

  WAVEFORMATEX requested_wave = make_wave_format(format);
  WAVEFORMATEX* mix_wave_ptr = nullptr;
  if (FAILED(audio_client->GetMixFormat(&mix_wave_ptr)) || !mix_wave_ptr) {
    ds5_set_last_error_message("Failed to query audio mix format");
    return DS5_E_AUDIO;
  }

  WAVEFORMATEX* wave_to_use = &requested_wave;
  WAVEFORMATEX* closest = nullptr;
  HRESULT support = audio_client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &requested_wave, &closest);
  CoTaskMemFree(closest);
  std::vector<uint8_t> converted;
  const void* playback_pcm = pcm;
  uint32_t playback_bytes = bytes;

  if (support != S_OK) {
    converted = convert_s16_pcm_to_mix(pcm, bytes, format, mix_wave_ptr);
    if (converted.empty()) {
      CoTaskMemFree(mix_wave_ptr);
      ds5_set_last_error_message("Audio endpoint mix format is incompatible with supplied PCM format");
      return DS5_E_AUDIO;
    }
    wave_to_use = mix_wave_ptr;
    playback_pcm = converted.data();
    playback_bytes = static_cast<uint32_t>(converted.size());
  }

  REFERENCE_TIME buffer_duration = 10000000;
  if (FAILED(audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, buffer_duration, 0, wave_to_use, nullptr))) {
    CoTaskMemFree(mix_wave_ptr);
    ds5_set_last_error_message("Failed to initialize audio playback format");
    return DS5_E_AUDIO;
  }

  ComPtr<IAudioRenderClient> render_client;
  if (FAILED(audio_client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(render_client.put())))) {
    ds5_set_last_error_message("Failed to acquire IAudioRenderClient");
    return DS5_E_AUDIO;
  }

  UINT32 buffer_frames = 0;
  audio_client->GetBufferSize(&buffer_frames);
  const uint8_t* source = static_cast<const uint8_t*>(playback_pcm);
  uint32_t remaining = playback_bytes;
  const uint32_t frame_bytes = wave_to_use->nBlockAlign;
  uint32_t written_frames_total = 0;

  if (FAILED(audio_client->Start())) {
    CoTaskMemFree(mix_wave_ptr);
    ds5_set_last_error_message("Failed to start audio playback");
    return DS5_E_AUDIO;
  }

  while (remaining > 0u) {
    UINT32 padding = 0;
    audio_client->GetCurrentPadding(&padding);
    UINT32 available_frames = buffer_frames - padding;
    if (available_frames == 0u) {
      std::this_thread::sleep_for(std::chrono::milliseconds(4));
      continue;
    }

    UINT32 frames_to_write = std::min<UINT32>(available_frames, remaining / frame_bytes);
    if (frames_to_write == 0u) {
      break;
    }
    BYTE* destination = nullptr;
    if (FAILED(render_client->GetBuffer(frames_to_write, &destination))) {
      audio_client->Stop();
      CoTaskMemFree(mix_wave_ptr);
      ds5_set_last_error_message("Failed to acquire audio render buffer");
      return DS5_E_AUDIO;
    }
    const uint32_t copy_bytes = frames_to_write * frame_bytes;
    std::memcpy(destination, source, copy_bytes);
    render_client->ReleaseBuffer(frames_to_write, 0);
    source += copy_bytes;
    remaining -= copy_bytes;
    written_frames_total += frames_to_write;
  }

  // Shared-mode render clients queue data faster than the endpoint can play it.
  // Wait for the queued frames to drain before Stop(), otherwise short tones can
  // be discarded before the DualSense speaker makes an audible sound.
  const uint32_t expected_ms = wave_to_use->nSamplesPerSec > 0
                                  ? (written_frames_total * 1000u) / wave_to_use->nSamplesPerSec
                                  : 0u;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(expected_ms + 500u);
  while (std::chrono::steady_clock::now() < deadline) {
    UINT32 padding = 0;
    if (FAILED(audio_client->GetCurrentPadding(&padding)) || padding == 0u) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
  }
  audio_client->Stop();
  CoTaskMemFree(mix_wave_ptr);
  return DS5_OK;
}

ds5_result ds5_audio_capture_start(ds5_context* context, const char* endpoint_id_value, const ds5_audio_format* preferred_format,
                                   ds5_audio_capture_callback callback, void* user_data, ds5_audio_capture** capture) {
  (void)context;
  if (!callback || !capture) {
    ds5_set_last_error_message("ds5_audio_capture_start requires callback and capture output");
    return DS5_E_INVALID_ARGUMENT;
  }
  if (preferred_format && !ds5_validate_struct(preferred_format->size, preferred_format->version, sizeof(ds5_audio_format))) {
    ds5_set_last_error_message("ds5_audio_capture_start received an uninitialized audio format");
    return DS5_E_INVALID_ARGUMENT;
  }

  ds5_audio_format callback_format{};
  ds5_audio_format_init(&callback_format,
                        preferred_format && preferred_format->sample_rate ? preferred_format->sample_rate : 48000u,
                        preferred_format && preferred_format->channels ? preferred_format->channels : 1u,
                        preferred_format && preferred_format->bits_per_sample ? preferred_format->bits_per_sample : 16u);

  auto* state = new ds5_audio_capture();
  state->running = true;
  std::string endpoint_id_copy = endpoint_id_value ? endpoint_id_value : "";
  state->worker = std::thread([state, endpoint_id_copy, callback_format, callback, user_data]() {
    ComInit com;
    if (FAILED(com.hr())) {
      return;
    }

    ComPtr<IMMDeviceEnumerator> enumerator;
    if (create_enumerator(enumerator) != DS5_OK) {
      return;
    }

    ComPtr<IMMDevice> device;
    if (get_device_by_id_or_default(enumerator.get(), endpoint_id_copy.c_str(), eCapture, device) != DS5_OK) {
      return;
    }

    ComPtr<IAudioClient> audio_client;
    if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(audio_client.put())))) {
      return;
    }

    WAVEFORMATEX wave = make_wave_format(&callback_format);
    if (FAILED(audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, &wave, nullptr))) {
      return;
    }

    ComPtr<IAudioCaptureClient> capture_client;
    if (FAILED(audio_client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(capture_client.put())))) {
      return;
    }

    if (FAILED(audio_client->Start())) {
      return;
    }

    while (state->running) {
      UINT32 packet_frames = 0;
      if (FAILED(capture_client->GetNextPacketSize(&packet_frames)) || packet_frames == 0u) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        continue;
      }

      BYTE* data = nullptr;
      UINT32 frames = 0;
      DWORD flags = 0;
      if (SUCCEEDED(capture_client->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) {
        if (data && frames > 0u && (flags & AUDCLNT_BUFFERFLAGS_SILENT) == 0u) {
          callback(data, frames * wave.nBlockAlign, &callback_format, user_data);
        }
        capture_client->ReleaseBuffer(frames);
      }
    }
    audio_client->Stop();
  });

  *capture = state;
  return DS5_OK;
}

void ds5_audio_capture_stop(ds5_audio_capture* capture) {
  if (!capture) {
    return;
  }
  capture->running = false;
  if (capture->worker.joinable()) {
    capture->worker.join();
  }
  delete capture;
}
