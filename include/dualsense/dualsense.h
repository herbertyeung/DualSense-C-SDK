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
#define DS5_MAX_PATH 260u
#define DS5_MAX_NAME 128u
#define DS5_RAW_REPORT_MAX 128u
#define DS5_AUDIO_ENDPOINT_ID_MAX 260u

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
  DS5_E_NOT_IMPLEMENTED = -10
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

typedef struct ds5_capabilities {
  uint32_t size;
  uint32_t version;
  uint32_t flags;
  ds5_transport transport;
} ds5_capabilities;

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

typedef struct ds5_touch_point {
  uint8_t active;
  uint8_t id;
  uint16_t x;
  uint16_t y;
} ds5_touch_point;

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

typedef struct ds5_trigger_effect {
  /* Compact v1 trigger description. Unsupported advanced modes can be sent with raw reports. */
  ds5_trigger_effect_mode mode;
  uint8_t start_position;
  uint8_t end_position;
  uint8_t force;
  uint8_t frequency;
} ds5_trigger_effect;

typedef struct ds5_audio_endpoint {
  uint32_t size;
  uint32_t version;
  uint32_t is_capture;
  uint32_t is_default;
  char id[DS5_AUDIO_ENDPOINT_ID_MAX];
  char name[DS5_MAX_NAME];
} ds5_audio_endpoint;

typedef struct ds5_audio_format {
  uint32_t size;
  uint32_t version;
  uint32_t sample_rate;
  uint16_t channels;
  uint16_t bits_per_sample;
} ds5_audio_format;

typedef void (*ds5_log_callback)(int level, const char* message, void* user_data);
typedef void (*ds5_audio_capture_callback)(const void* pcm, uint32_t bytes, const ds5_audio_format* format, void* user_data);

DS5_API ds5_result ds5_init(ds5_context** context);
DS5_API void ds5_shutdown(ds5_context* context);
DS5_API void ds5_set_log_callback(ds5_context* context, ds5_log_callback callback, void* user_data);
DS5_API const char* ds5_get_last_error(void);

DS5_API ds5_result ds5_enumerate(ds5_context* context, ds5_device_info* devices, uint32_t capacity, uint32_t* count);
DS5_API ds5_result ds5_open(ds5_context* context, const ds5_device_info* info, ds5_device** device);
DS5_API void ds5_close(ds5_device* device);
DS5_API ds5_result ds5_get_capabilities(ds5_device* device, ds5_capabilities* capabilities);
DS5_API ds5_result ds5_poll_state(ds5_device* device, ds5_state* state);

/* Output APIs are implemented for USB transport in v1 and return DS5_E_UNSUPPORTED_TRANSPORT for Bluetooth. */
DS5_API ds5_result ds5_set_lightbar(ds5_device* device, uint8_t r, uint8_t g, uint8_t b);
DS5_API ds5_result ds5_set_player_leds(ds5_device* device, uint8_t mask);
DS5_API ds5_result ds5_set_mic_led(ds5_device* device, ds5_mic_led mode);
DS5_API ds5_result ds5_set_rumble(ds5_device* device, uint8_t left, uint8_t right);
DS5_API ds5_result ds5_set_haptic_pattern(ds5_device* device, uint8_t left, uint8_t right, uint32_t duration_ms);
DS5_API ds5_result ds5_set_trigger_effect(ds5_device* device, uint32_t left_trigger, const ds5_trigger_effect* effect);
/* Advanced USB-only escape hatch for callers that need to experiment with HID output reports directly. */
DS5_API ds5_result ds5_send_raw_output_report(ds5_device* device, const void* bytes, uint32_t size);

/* Audio helpers use Windows MMDevice/WASAPI endpoint IDs and do not change system default devices. */
DS5_API ds5_result ds5_audio_enumerate_endpoints(ds5_context* context, ds5_audio_endpoint* endpoints, uint32_t capacity, uint32_t* count);
DS5_API ds5_result ds5_audio_play_pcm(ds5_context* context, const char* endpoint_id, const void* pcm, uint32_t bytes, const ds5_audio_format* format);
DS5_API ds5_result ds5_audio_capture_start(ds5_context* context, const char* endpoint_id, const ds5_audio_format* preferred_format, ds5_audio_capture_callback callback, void* user_data, ds5_audio_capture** capture);
DS5_API void ds5_audio_capture_stop(ds5_audio_capture* capture);

#ifdef __cplusplus
}
#endif

#endif
