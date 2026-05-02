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

bool ds5_validate_struct(uint32_t size, uint32_t minimum_size) {
  return size >= minimum_size;
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
  if (!device || !capabilities || !ds5_validate_struct(capabilities->size, sizeof(ds5_capabilities))) {
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
