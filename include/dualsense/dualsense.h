/*
  File: dualsense.h
  Author: Herbert Yeung
  Purpose: Public C ABI for the Windows DualSense SDK.
*/

#ifndef DUALSENSE_DUALSENSE_H
#define DUALSENSE_DUALSENSE_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  if defined(DS5_BUILDING_DLL)
#    define DS5_API __declspec(dllexport)
#  elif defined(DS5_USE_DLL)
#    define DS5_API __declspec(dllimport)
#  else
#    define DS5_API
#  endif
#else
#  define DS5_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define DS5_STRUCT_VERSION 1u
#define DS5_VERSION_MAJOR 0u
#define DS5_VERSION_MINOR 2u
#define DS5_VERSION_PATCH 0u
#define DS5_VERSION_STRING "0.2.0"
#define DS5_MAX_PATH 260u
#define DS5_MAX_NAME 128u
#define DS5_RAW_REPORT_MAX 128u
#define DS5_AUDIO_ENDPOINT_ID_MAX 260u

/*
  DualSense SDK public C ABI.

  The C API is the stable product boundary. All handles are opaque, and all
  caller-owned structs start with `size` and `version` so future SDK versions
  can extend them while preserving v1 binary compatibility.

  General call order:
    1. ds5_init(&context)
    2. ds5_enumerate(context, ...)
    3. ds5_open(context, &info, &device)
    4. ds5_poll_state / output / audio calls
    5. ds5_close(device)
    6. ds5_shutdown(context)

  Threading:
    - Distinct devices can be used from different threads.
    - Output state on one device is internally serialized.
    - ds5_get_last_error() returns thread-local text for the most recent
      failing SDK call on the current thread.

  Transport:
    - USB is the full-capability v1 transport.
    - Bluetooth is intentionally reduced in v1; output APIs return
      DS5_E_UNSUPPORTED_TRANSPORT when the transport is not supported.
*/

typedef enum ds5_result {
  DS5_OK = 0,
  DS5_E_INVALID_ARGUMENT = -1,
  DS5_E_NOT_INITIALIZED = -2,
  DS5_E_NOT_FOUND = -3,
  DS5_E_OPEN_FAILED = -4,
  DS5_E_IO = -5,
  DS5_E_UNSUPPORTED_TRANSPORT = -6,
  DS5_E_DEVICE_CAPABILITY = -7,
  DS5_E_INSUFFICIENT_BUFFER = -8,
  DS5_E_AUDIO = -9,
  DS5_E_NOT_IMPLEMENTED = -10,
  DS5_E_TIMEOUT = -11
} ds5_result;

typedef enum ds5_transport {
  DS5_TRANSPORT_UNKNOWN = 0,
  DS5_TRANSPORT_USB = 1,
  DS5_TRANSPORT_BLUETOOTH = 2
} ds5_transport;

typedef enum ds5_capability_flags {
  DS5_CAP_INPUT = 1u << 0,
  DS5_CAP_LIGHTBAR = 1u << 1,
  DS5_CAP_PLAYER_LEDS = 1u << 2,
  DS5_CAP_MIC_LED = 1u << 3,
  DS5_CAP_CLASSIC_RUMBLE = 1u << 4,
  DS5_CAP_HAPTICS = 1u << 5,
  DS5_CAP_ADAPTIVE_TRIGGERS = 1u << 6,
  DS5_CAP_AUDIO_SPEAKER = 1u << 7,
  DS5_CAP_AUDIO_MICROPHONE = 1u << 8,
  DS5_CAP_HEADSET_JACK = 1u << 9,
  DS5_CAP_TOUCHPAD = 1u << 10,
  DS5_CAP_IMU = 1u << 11,
  DS5_CAP_RAW_REPORTS = 1u << 12
} ds5_capability_flags;

typedef enum ds5_buttons {
  DS5_BUTTON_SQUARE = 1u << 0,
  DS5_BUTTON_CROSS = 1u << 1,
  DS5_BUTTON_CIRCLE = 1u << 2,
  DS5_BUTTON_TRIANGLE = 1u << 3,
  DS5_BUTTON_L1 = 1u << 4,
  DS5_BUTTON_R1 = 1u << 5,
  DS5_BUTTON_L2 = 1u << 6,
  DS5_BUTTON_R2 = 1u << 7,
  DS5_BUTTON_CREATE = 1u << 8,
  DS5_BUTTON_OPTIONS = 1u << 9,
  DS5_BUTTON_L3 = 1u << 10,
  DS5_BUTTON_R3 = 1u << 11,
  DS5_BUTTON_PS = 1u << 12,
  DS5_BUTTON_TOUCHPAD = 1u << 13,
  DS5_BUTTON_MUTE = 1u << 14
} ds5_buttons;

typedef enum ds5_dpad {
  DS5_DPAD_UP = 0,
  DS5_DPAD_UP_RIGHT = 1,
  DS5_DPAD_RIGHT = 2,
  DS5_DPAD_DOWN_RIGHT = 3,
  DS5_DPAD_DOWN = 4,
  DS5_DPAD_DOWN_LEFT = 5,
  DS5_DPAD_LEFT = 6,
  DS5_DPAD_UP_LEFT = 7,
  DS5_DPAD_NONE = 8
} ds5_dpad;

typedef enum ds5_mic_led {
  DS5_MIC_LED_OFF = 0,
  DS5_MIC_LED_ON = 1,
  DS5_MIC_LED_PULSE = 2
} ds5_mic_led;

typedef enum ds5_trigger_effect_mode {
  DS5_TRIGGER_EFFECT_OFF = 0,
  DS5_TRIGGER_EFFECT_CONSTANT_RESISTANCE = 1,
  DS5_TRIGGER_EFFECT_SECTION_RESISTANCE = 2,
  DS5_TRIGGER_EFFECT_WEAPON = 3,
  DS5_TRIGGER_EFFECT_VIBRATION = 4
} ds5_trigger_effect_mode;

typedef struct ds5_context ds5_context;
typedef struct ds5_device ds5_device;
typedef struct ds5_audio_capture ds5_audio_capture;

/* Capability summary for one opened/enumerated controller. */
typedef struct ds5_capabilities {
  uint32_t size;
  uint32_t version;
  /* Bitwise OR of ds5_capability_flags. Check this before enabling optional features. */
  uint32_t flags;
  /* Transport used for the device that produced these capabilities. */
  ds5_transport transport;
} ds5_capabilities;

/* Device descriptor returned by ds5_enumerate and accepted by ds5_open. */
typedef struct ds5_device_info {
  uint32_t size;
  uint32_t version;
  uint16_t vendor_id;
  uint16_t product_id;
  ds5_transport transport;
  char path[DS5_MAX_PATH];
  char serial[DS5_MAX_NAME];
  char product[DS5_MAX_NAME];
  ds5_capabilities capabilities;
} ds5_device_info;

/* One finger on the touchpad. `active == 0` means the finger is not touching. */
typedef struct ds5_touch_point {
  uint8_t active;
  uint8_t id;
  uint16_t x;
  uint16_t y;
} ds5_touch_point;

/*
  Snapshot from the latest HID input report.

  Sticks and triggers are raw 0-255 controller values. IMU values are raw signed
  samples from the input report; applications that need stable motion control
  should apply their own calibration/filtering. `raw_report` is included for
  diagnostics and protocol experiments, not for normal gameplay code.
*/
typedef struct ds5_state {
  uint32_t size;
  uint32_t version;
  uint32_t buttons;
  ds5_dpad dpad;
  uint8_t left_stick_x;
  uint8_t left_stick_y;
  uint8_t right_stick_x;
  uint8_t right_stick_y;
  uint8_t left_trigger;
  uint8_t right_trigger;
  int16_t gyro_x;
  int16_t gyro_y;
  int16_t gyro_z;
  int16_t accel_x;
  int16_t accel_y;
  int16_t accel_z;
  uint8_t battery_percent;
  ds5_touch_point touch[2];
  ds5_transport transport;
  uint32_t raw_report_size;
  /* Last HID input report bytes, preserved for diagnostics and protocol experiments. */
  uint8_t raw_report[DS5_RAW_REPORT_MAX];
} ds5_state;

/*
  Compact v1 adaptive-trigger description.

  Use the ds5_trigger_effect_* helper functions when possible. The raw fields
  are kept public so advanced callers can still persist, inspect, or bind the
  structure from other languages.
*/
typedef struct ds5_trigger_effect {
  ds5_trigger_effect_mode mode;
  uint8_t start_position;
  uint8_t end_position;
  uint8_t force;
  uint8_t frequency;
} ds5_trigger_effect;

/* Windows MMDevice/WASAPI endpoint that appears to belong to the controller. */
typedef struct ds5_audio_endpoint {
  uint32_t size;
  uint32_t version;
  uint32_t is_capture;
  uint32_t is_default;
  char id[DS5_AUDIO_ENDPOINT_ID_MAX];
  char name[DS5_MAX_NAME];
} ds5_audio_endpoint;

/* PCM format used by the simple playback/capture helpers. */
typedef struct ds5_audio_format {
  uint32_t size;
  uint32_t version;
  uint32_t sample_rate;
  uint16_t channels;
  uint16_t bits_per_sample;
} ds5_audio_format;

typedef void (*ds5_log_callback)(int level, const char* message, void* user_data);
typedef void (*ds5_audio_capture_callback)(const void* pcm, uint32_t bytes, const ds5_audio_format* format, void* user_data);

/* Returns the runtime SDK version as `major.minor.patch`. Output pointers may be NULL. */
DS5_API void ds5_get_version(uint32_t* major, uint32_t* minor, uint32_t* patch);
/* Returns the runtime SDK version string. The returned pointer is static storage. */
DS5_API const char* ds5_get_version_string(void);
/* Returns a stable English name for a ds5_result value. The returned pointer is static storage. */
DS5_API const char* ds5_result_to_string(ds5_result result);

/* Initializes a caller-owned struct with the current v1 size/version fields. */
DS5_API void ds5_capabilities_init(ds5_capabilities* capabilities);
DS5_API void ds5_device_info_init(ds5_device_info* info);
DS5_API void ds5_state_init(ds5_state* state);
DS5_API void ds5_audio_endpoint_init(ds5_audio_endpoint* endpoint);
/* Initializes an audio format. Passing zeros selects the SDK defaults: 48000 Hz, stereo, 16-bit PCM. */
DS5_API void ds5_audio_format_init(ds5_audio_format* format, uint32_t sample_rate, uint16_t channels, uint16_t bits_per_sample);

/* Initializes a trigger effect to off. */
DS5_API void ds5_trigger_effect_init(ds5_trigger_effect* effect);
DS5_API void ds5_trigger_effect_off(ds5_trigger_effect* effect);
/* Constant resistance from `start_position` to the end of trigger travel. */
DS5_API void ds5_trigger_effect_constant_resistance(ds5_trigger_effect* effect, uint8_t start_position, uint8_t force);
/* Resistance over a bounded trigger section. */
DS5_API void ds5_trigger_effect_section_resistance(ds5_trigger_effect* effect, uint8_t start_position, uint8_t end_position, uint8_t force);
/* Weapon-style break point effect. */
DS5_API void ds5_trigger_effect_weapon(ds5_trigger_effect* effect, uint8_t start_position, uint8_t end_position, uint8_t force);
/* Trigger vibration effect. */
DS5_API void ds5_trigger_effect_vibration(ds5_trigger_effect* effect, uint8_t start_position, uint8_t end_position, uint8_t force, uint8_t frequency);

/* Creates a context. A process may own more than one context, but one is enough for most apps. */
DS5_API ds5_result ds5_init(ds5_context** context);
/* Destroys a context. Close all devices and stop audio captures created from it first. */
DS5_API void ds5_shutdown(ds5_context* context);
/* Installs an optional log callback. The callback is invoked synchronously from SDK calls. */
DS5_API void ds5_set_log_callback(ds5_context* context, ds5_log_callback callback, void* user_data);
/* Returns thread-local text for the most recent failing SDK call on this thread. */
DS5_API const char* ds5_get_last_error(void);

/*
  Enumerates currently present DualSense HID devices.

  To query the required count, call with `devices == NULL` and `capacity == 0`.
  If devices are found, that first call returns DS5_E_INSUFFICIENT_BUFFER and
  writes the count. Allocate that many ds5_device_info entries, initialize each
  with ds5_device_info_init, then call again.
*/
DS5_API ds5_result ds5_enumerate(ds5_context* context, ds5_device_info* devices, uint32_t capacity, uint32_t* count);
/* Opens a device returned by ds5_enumerate. The device must be closed with ds5_close. */
DS5_API ds5_result ds5_open(ds5_context* context, const ds5_device_info* info, ds5_device** device);
/* Closes an opened controller. NULL is allowed. */
DS5_API void ds5_close(ds5_device* device);
/* Copies the opened device capability flags into `capabilities`. */
DS5_API ds5_result ds5_get_capabilities(ds5_device* device, ds5_capabilities* capabilities);
/* Blocking read of the next input report. Use from a polling/input thread, not a latency-sensitive UI callback. */
DS5_API ds5_result ds5_poll_state(ds5_device* device, ds5_state* state);
/* Reads the next input report, waiting at most `timeout_ms`. Returns DS5_E_TIMEOUT when no report arrives in time. */
DS5_API ds5_result ds5_poll_state_timeout(ds5_device* device, uint32_t timeout_ms, ds5_state* state);
/* Nonblocking input poll. Returns DS5_E_TIMEOUT when no input report is ready immediately. */
DS5_API ds5_result ds5_try_poll_state(ds5_device* device, ds5_state* state);

/* Output APIs are implemented for USB transport in v1 and return DS5_E_UNSUPPORTED_TRANSPORT for Bluetooth. */
/* Sets the RGB lightbar color. */
DS5_API ds5_result ds5_set_lightbar(ds5_device* device, uint8_t r, uint8_t g, uint8_t b);
/* Sets the five player LEDs with a low-five-bit mask. */
DS5_API ds5_result ds5_set_player_leds(ds5_device* device, uint8_t mask);
/* Sets the microphone LED mode. */
DS5_API ds5_result ds5_set_mic_led(ds5_device* device, ds5_mic_led mode);
/* Sets classic compatible rumble motors. Values are 0-255. */
DS5_API ds5_result ds5_set_rumble(ds5_device* device, uint8_t left, uint8_t right);
/* Blocking helper: sets rumble, sleeps for duration_ms, then clears rumble. */
DS5_API ds5_result ds5_set_haptic_pattern(ds5_device* device, uint8_t left, uint8_t right, uint32_t duration_ms);
/* Sets one adaptive trigger effect. Pass left_trigger != 0 for L2, 0 for R2. */
DS5_API ds5_result ds5_set_trigger_effect(ds5_device* device, uint32_t left_trigger, const ds5_trigger_effect* effect);
/* Clears rumble, adaptive triggers, and mic LED with one output report. */
DS5_API ds5_result ds5_reset_feedback(ds5_device* device);
/* Advanced USB-only escape hatch for callers that need to experiment with HID output reports directly. */
DS5_API ds5_result ds5_send_raw_output_report(ds5_device* device, const void* bytes, uint32_t size);

/* Audio helpers use Windows MMDevice/WASAPI endpoint IDs and do not change system default devices. */
/* Enumerates active render/capture endpoints whose names look like DualSense endpoints. */
DS5_API ds5_result ds5_audio_enumerate_endpoints(ds5_context* context, ds5_audio_endpoint* endpoints, uint32_t capacity, uint32_t* count);
/* Blocking shared-mode WASAPI playback of PCM data to an explicit endpoint id, or default render endpoint when NULL/empty. */
DS5_API ds5_result ds5_audio_play_pcm(ds5_context* context, const char* endpoint_id, const void* pcm, uint32_t bytes, const ds5_audio_format* format);
/* Starts background capture from an explicit endpoint id, or default capture endpoint when NULL/empty. */
DS5_API ds5_result ds5_audio_capture_start(ds5_context* context, const char* endpoint_id, const ds5_audio_format* preferred_format, ds5_audio_capture_callback callback, void* user_data, ds5_audio_capture** capture);
/* Stops capture, joins the worker thread, and frees the capture handle. NULL is allowed. */
DS5_API void ds5_audio_capture_stop(ds5_audio_capture* capture);

#ifdef __cplusplus
}
#endif

#endif
