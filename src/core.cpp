#include "core.h"

#include <combaseapi.h>

#include <algorithm>
#include <cstdio>
#include <vector>

namespace {
thread_local std::string g_last_error;
}

void ds5_set_last_error_message(const std::string& message) {
  g_last_error = message;
}

const char* ds5_get_last_error(void) {
  return g_last_error.empty() ? "DS5_OK" : g_last_error.c_str();
}

void ds5_get_version(uint32_t* major, uint32_t* minor, uint32_t* patch) {
  if (major) *major = DS5_VERSION_MAJOR;
  if (minor) *minor = DS5_VERSION_MINOR;
  if (patch) *patch = DS5_VERSION_PATCH;
}

const char* ds5_get_version_string(void) {
  return DS5_VERSION_STRING;
}

const char* ds5_result_to_string(ds5_result result) {
  switch (result) {
    case DS5_OK: return "DS5_OK";
    case DS5_E_INVALID_ARGUMENT: return "DS5_E_INVALID_ARGUMENT";
    case DS5_E_NOT_INITIALIZED: return "DS5_E_NOT_INITIALIZED";
    case DS5_E_NOT_FOUND: return "DS5_E_NOT_FOUND";
    case DS5_E_OPEN_FAILED: return "DS5_E_OPEN_FAILED";
    case DS5_E_IO: return "DS5_E_IO";
    case DS5_E_UNSUPPORTED_TRANSPORT: return "DS5_E_UNSUPPORTED_TRANSPORT";
    case DS5_E_DEVICE_CAPABILITY: return "DS5_E_DEVICE_CAPABILITY";
    case DS5_E_INSUFFICIENT_BUFFER: return "DS5_E_INSUFFICIENT_BUFFER";
    case DS5_E_AUDIO: return "DS5_E_AUDIO";
    case DS5_E_NOT_IMPLEMENTED: return "DS5_E_NOT_IMPLEMENTED";
    default: return "DS5_E_UNKNOWN";
  }
}

void ds5_capabilities_init(ds5_capabilities* capabilities) {
  if (!capabilities) return;
  *capabilities = {};
  capabilities->size = sizeof(*capabilities);
  capabilities->version = DS5_STRUCT_VERSION;
}

void ds5_device_info_init(ds5_device_info* info) {
  if (!info) return;
  *info = {};
  info->size = sizeof(*info);
  info->version = DS5_STRUCT_VERSION;
  ds5_capabilities_init(&info->capabilities);
}

void ds5_state_init(ds5_state* state) {
  if (!state) return;
  *state = {};
  state->size = sizeof(*state);
  state->version = DS5_STRUCT_VERSION;
  state->dpad = DS5_DPAD_NONE;
}

void ds5_audio_endpoint_init(ds5_audio_endpoint* endpoint) {
  if (!endpoint) return;
  *endpoint = {};
  endpoint->size = sizeof(*endpoint);
  endpoint->version = DS5_STRUCT_VERSION;
}

void ds5_audio_format_init(ds5_audio_format* format, uint32_t sample_rate, uint16_t channels, uint16_t bits_per_sample) {
  if (!format) return;
  *format = {};
  format->size = sizeof(*format);
  format->version = DS5_STRUCT_VERSION;
  format->sample_rate = sample_rate ? sample_rate : 48000u;
  format->channels = channels ? channels : 2u;
  format->bits_per_sample = bits_per_sample ? bits_per_sample : 16u;
}

void ds5_trigger_effect_init(ds5_trigger_effect* effect) {
  ds5_trigger_effect_off(effect);
}

void ds5_trigger_effect_off(ds5_trigger_effect* effect) {
  if (!effect) return;
  *effect = {};
  effect->mode = DS5_TRIGGER_EFFECT_OFF;
}

void ds5_trigger_effect_constant_resistance(ds5_trigger_effect* effect, uint8_t start_position, uint8_t force) {
  ds5_trigger_effect_off(effect);
  if (!effect) return;
  effect->mode = DS5_TRIGGER_EFFECT_CONSTANT_RESISTANCE;
  effect->start_position = start_position;
  effect->force = force;
}

void ds5_trigger_effect_section_resistance(ds5_trigger_effect* effect, uint8_t start_position, uint8_t end_position, uint8_t force) {
  ds5_trigger_effect_off(effect);
  if (!effect) return;
  effect->mode = DS5_TRIGGER_EFFECT_SECTION_RESISTANCE;
  effect->start_position = start_position;
  effect->end_position = end_position;
  effect->force = force;
}

void ds5_trigger_effect_weapon(ds5_trigger_effect* effect, uint8_t start_position, uint8_t end_position, uint8_t force) {
  ds5_trigger_effect_off(effect);
  if (!effect) return;
  effect->mode = DS5_TRIGGER_EFFECT_WEAPON;
  effect->start_position = start_position;
  effect->end_position = end_position;
  effect->force = force;
}

void ds5_trigger_effect_vibration(ds5_trigger_effect* effect, uint8_t start_position, uint8_t end_position, uint8_t force, uint8_t frequency) {
  ds5_trigger_effect_off(effect);
  if (!effect) return;
  effect->mode = DS5_TRIGGER_EFFECT_VIBRATION;
  effect->start_position = start_position;
  effect->end_position = end_position;
  effect->force = force;
  effect->frequency = frequency;
}

void ds5_log(ds5_context* context, int level, const std::string& message) {
  if (context && context->log_callback) {
    context->log_callback(level, message.c_str(), context->log_user_data);
  }
}

std::string ds5_wide_to_utf8(const wchar_t* value) {
  if (!value) {
    return {};
  }
  const int required = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
  if (required <= 1) {
    return {};
  }
  std::string result(static_cast<size_t>(required - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value, -1, &result[0], required, nullptr, nullptr);
  return result;
}

std::wstring ds5_utf8_to_wide(const char* value) {
  if (!value) {
    return {};
  }
  const int required = MultiByteToWideChar(CP_UTF8, 0, value, -1, nullptr, 0);
  if (required <= 1) {
    return {};
  }
  std::wstring result(static_cast<size_t>(required - 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value, -1, &result[0], required);
  return result;
}

bool ds5_validate_struct(uint32_t size, uint32_t version, uint32_t minimum_size) {
  return size >= minimum_size && version == DS5_STRUCT_VERSION;
}

ds5_result ds5_init(ds5_context** context) {
  if (!context) {
    ds5_set_last_error_message("ds5_init requires a context output pointer");
    return DS5_E_INVALID_ARGUMENT;
  }
  *context = new ds5_context();
  ds5_set_last_error_message({});
  return DS5_OK;
}

void ds5_shutdown(ds5_context* context) {
  delete context;
}

void ds5_set_log_callback(ds5_context* context, ds5_log_callback callback, void* user_data) {
  if (!context) {
    return;
  }
  std::lock_guard<std::mutex> lock(context->mutex);
  context->log_callback = callback;
  context->log_user_data = user_data;
}

ds5_result ds5_get_capabilities(ds5_device* device, ds5_capabilities* capabilities) {
  if (!device || !capabilities || !ds5_validate_struct(capabilities->size, capabilities->version, sizeof(ds5_capabilities))) {
    ds5_set_last_error_message("ds5_get_capabilities received invalid arguments");
    return DS5_E_INVALID_ARGUMENT;
  }
  *capabilities = device->info.capabilities;
  return DS5_OK;
}

void ds5_close(ds5_device* device) {
  if (!device) {
    return;
  }
  if (device->handle != INVALID_HANDLE_VALUE) {
    CloseHandle(device->handle);
  }
  delete device;
}
