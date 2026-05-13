/*
  File: core.h
  Author: Herbert Yeung
  Purpose: Internal context, device, error, and shared utility declarations.
*/

#ifndef DS5_CORE_H
#define DS5_CORE_H

#include <Windows.h>

#include <mutex>
#include <string>

#include <dualsense/dualsense.h>

#include "output_report.h"

struct ds5_context {
  ds5_log_callback log_callback = nullptr;
  void* log_user_data = nullptr;
  std::mutex mutex;
};

struct ds5_device {
  ds5_context* context = nullptr;
  HANDLE handle = INVALID_HANDLE_VALUE;
  ds5_device_info info{};
  ds5_output_state output{};
  std::mutex mutex;
  std::mutex input_mutex;
};

void ds5_set_last_error_message(const std::string& message);
void ds5_log(ds5_context* context, int level, const std::string& message);
std::string ds5_wide_to_utf8(const wchar_t* value);
std::wstring ds5_utf8_to_wide(const char* value);
bool ds5_validate_struct(uint32_t size, uint32_t version, uint32_t minimum_size);
ds5_result ds5_write_current_output(ds5_device* device);

#endif
