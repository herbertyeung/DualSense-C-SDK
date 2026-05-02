#include "core.h"

#include <hidsdi.h>
#include <setupapi.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "report_parser.h"

namespace {

constexpr uint16_t kSonyVid = 0x054c;
constexpr uint16_t kDualSensePid = 0x0ce6;
constexpr uint16_t kDualSenseEdgePid = 0x0df2;

bool is_supported_pid(uint16_t pid) {
  return pid == kDualSensePid || pid == kDualSenseEdgePid;
}

std::string lower_copy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

ds5_transport transport_from_path(const std::string& path) {
  const std::string lowered = lower_copy(path);
  if (lowered.find("bth") != std::string::npos || lowered.find("bluetooth") != std::string::npos) {
    return DS5_TRANSPORT_BLUETOOTH;
  }
  if (lowered.find("usb") != std::string::npos || lowered.find("vid_054c") != std::string::npos) {
    return DS5_TRANSPORT_USB;
  }
  return DS5_TRANSPORT_UNKNOWN;
}

void copy_string(char* target, size_t target_size, const std::string& source) {
  if (!target || target_size == 0u) {
    return;
  }
  std::memset(target, 0, target_size);
  const size_t count = std::min(target_size - 1u, source.size());
  std::memcpy(target, source.data(), count);
}

std::string product_string(HANDLE handle) {
  wchar_t buffer[DS5_MAX_NAME]{};
  if (HidD_GetProductString(handle, buffer, sizeof(buffer))) {
    return ds5_wide_to_utf8(buffer);
  }
  return "DualSense Wireless Controller";
}

std::string serial_string(HANDLE handle) {
  wchar_t buffer[DS5_MAX_NAME]{};
  if (HidD_GetSerialNumberString(handle, buffer, sizeof(buffer))) {
    return ds5_wide_to_utf8(buffer);
  }
  return {};
}

ds5_result write_report(HANDLE handle, const uint8_t* bytes, uint32_t size) {
  DWORD written = 0;
  if (!WriteFile(handle, bytes, size, &written, nullptr) || written != size) {
    ds5_set_last_error_message("Failed to write DualSense output report");
    return DS5_E_IO;
  }
  return DS5_OK;
}

}  // namespace

ds5_result ds5_enumerate(ds5_context* context, ds5_device_info* devices, uint32_t capacity, uint32_t* count) {
  if (!context || !count) {
    ds5_set_last_error_message("ds5_enumerate received invalid arguments");
    return DS5_E_INVALID_ARGUMENT;
  }

  GUID hid_guid{};
  HidD_GetHidGuid(&hid_guid);

  HDEVINFO device_info_set = SetupDiGetClassDevsW(&hid_guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (device_info_set == INVALID_HANDLE_VALUE) {
    ds5_set_last_error_message("SetupDiGetClassDevsW failed");
    return DS5_E_IO;
  }

  std::vector<ds5_device_info> found;
  SP_DEVICE_INTERFACE_DATA interface_data{};
  interface_data.cbSize = sizeof(interface_data);

  for (DWORD index = 0; SetupDiEnumDeviceInterfaces(device_info_set, nullptr, &hid_guid, index, &interface_data); ++index) {
    DWORD required_size = 0;
    SetupDiGetDeviceInterfaceDetailW(device_info_set, &interface_data, nullptr, 0, &required_size, nullptr);
    if (required_size == 0) {
      continue;
    }

    std::vector<uint8_t> detail_buffer(required_size);
    auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(detail_buffer.data());
    detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
    if (!SetupDiGetDeviceInterfaceDetailW(device_info_set, &interface_data, detail, required_size, nullptr, nullptr)) {
      continue;
    }

    const std::string path = ds5_wide_to_utf8(detail->DevicePath);
    HANDLE handle = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
      continue;
    }

    HIDD_ATTRIBUTES attributes{};
    attributes.Size = sizeof(attributes);
    const bool attributes_ok = HidD_GetAttributes(handle, &attributes) != FALSE;
    if (attributes_ok && attributes.VendorID == kSonyVid && is_supported_pid(attributes.ProductID)) {
      ds5_device_info info{};
      info.size = sizeof(info);
      info.version = DS5_STRUCT_VERSION;
      info.vendor_id = attributes.VendorID;
      info.product_id = attributes.ProductID;
      info.transport = transport_from_path(path);
      info.capabilities = ds5_internal_capabilities_for_transport(info.transport);
      copy_string(info.path, sizeof(info.path), path);
      copy_string(info.product, sizeof(info.product), product_string(handle));
      copy_string(info.serial, sizeof(info.serial), serial_string(handle));
      found.push_back(info);
    }
    CloseHandle(handle);
  }

  SetupDiDestroyDeviceInfoList(device_info_set);

  *count = static_cast<uint32_t>(found.size());
  if (!devices || capacity < found.size()) {
    return found.empty() ? DS5_OK : DS5_E_INSUFFICIENT_BUFFER;
  }

  for (size_t i = 0; i < found.size(); ++i) {
    devices[i] = found[i];
  }
  return DS5_OK;
}

ds5_result ds5_open(ds5_context* context, const ds5_device_info* info, ds5_device** device) {
  if (!context || !info || !device) {
    ds5_set_last_error_message("ds5_open received invalid arguments");
    return DS5_E_INVALID_ARGUMENT;
  }
  if (info->transport == DS5_TRANSPORT_UNKNOWN) {
    ds5_set_last_error_message("Cannot open DualSense device with unknown transport");
    return DS5_E_UNSUPPORTED_TRANSPORT;
  }

  const std::wstring path = ds5_utf8_to_wide(info->path);
  HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == INVALID_HANDLE_VALUE && info->transport == DS5_TRANSPORT_BLUETOOTH) {
    // Bluetooth output reports need a transport-specific packet format. Open read-only
    // when Windows refuses write access so input polling still works.
    handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                         nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  }
  if (handle == INVALID_HANDLE_VALUE) {
    ds5_set_last_error_message("Failed to open DualSense HID device");
    return DS5_E_OPEN_FAILED;
  }

  auto* opened = new ds5_device();
  opened->context = context;
  opened->handle = handle;
  opened->info = *info;
  *device = opened;
  return DS5_OK;
}

ds5_result ds5_poll_state(ds5_device* device, ds5_state* state) {
  if (!device || !state || !ds5_validate_struct(state->size, sizeof(ds5_state))) {
    ds5_set_last_error_message("ds5_poll_state received invalid arguments");
    return DS5_E_INVALID_ARGUMENT;
  }

  uint8_t buffer[DS5_RAW_REPORT_MAX]{};
  DWORD read = 0;
  if (!ReadFile(device->handle, buffer, sizeof(buffer), &read, nullptr)) {
    ds5_set_last_error_message("Failed to read DualSense input report");
    return DS5_E_IO;
  }
  return ds5_internal_parse_input_report(buffer, read, device->info.transport, state);
}

ds5_result ds5_write_current_output(ds5_device* device) {
  if (!device) {
    ds5_set_last_error_message("Output write received null device");
    return DS5_E_INVALID_ARGUMENT;
  }
  if (device->info.transport != DS5_TRANSPORT_USB) {
    ds5_set_last_error_message("Output reports are implemented for USB DualSense transport only");
    return DS5_E_UNSUPPORTED_TRANSPORT;
  }
  ds5_internal_output_report report = ds5_internal_build_usb_output_report(&device->output);
  return write_report(device->handle, report.bytes, report.size);
}

ds5_result ds5_set_lightbar(ds5_device* device, uint8_t r, uint8_t g, uint8_t b) {
  if (!device) return DS5_E_INVALID_ARGUMENT;
  std::lock_guard<std::mutex> lock(device->mutex);
  device->output.lightbar_r = r;
  device->output.lightbar_g = g;
  device->output.lightbar_b = b;
  return ds5_write_current_output(device);
}

ds5_result ds5_set_player_leds(ds5_device* device, uint8_t mask) {
  if (!device) return DS5_E_INVALID_ARGUMENT;
  std::lock_guard<std::mutex> lock(device->mutex);
  device->output.player_leds = mask;
  return ds5_write_current_output(device);
}

ds5_result ds5_set_mic_led(ds5_device* device, ds5_mic_led mode) {
  if (!device) return DS5_E_INVALID_ARGUMENT;
  std::lock_guard<std::mutex> lock(device->mutex);
  device->output.mic_led = mode;
  return ds5_write_current_output(device);
}

ds5_result ds5_set_rumble(ds5_device* device, uint8_t left, uint8_t right) {
  if (!device) return DS5_E_INVALID_ARGUMENT;
  std::lock_guard<std::mutex> lock(device->mutex);
  device->output.left_rumble = left;
  device->output.right_rumble = right;
  return ds5_write_current_output(device);
}

ds5_result ds5_set_haptic_pattern(ds5_device* device, uint8_t left, uint8_t right, uint32_t duration_ms) {
  ds5_result result = ds5_set_rumble(device, left, right);
  if (result != DS5_OK || duration_ms == 0u) {
    return result;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
  return ds5_set_rumble(device, 0, 0);
}

ds5_result ds5_set_trigger_effect(ds5_device* device, uint32_t left_trigger, const ds5_trigger_effect* effect) {
  if (!device || !effect) return DS5_E_INVALID_ARGUMENT;
  std::lock_guard<std::mutex> lock(device->mutex);
  if (left_trigger) {
    device->output.left_trigger = *effect;
  } else {
    device->output.right_trigger = *effect;
  }
  return ds5_write_current_output(device);
}

ds5_result ds5_send_raw_output_report(ds5_device* device, const void* bytes, uint32_t size) {
  if (!device || !bytes || size == 0u) {
    ds5_set_last_error_message("ds5_send_raw_output_report received invalid arguments");
    return DS5_E_INVALID_ARGUMENT;
  }
  if (device->info.transport != DS5_TRANSPORT_USB) {
    ds5_set_last_error_message("Raw output reports are implemented for USB DualSense transport only");
    return DS5_E_UNSUPPORTED_TRANSPORT;
  }
  return write_report(device->handle, static_cast<const uint8_t*>(bytes), size);
}
