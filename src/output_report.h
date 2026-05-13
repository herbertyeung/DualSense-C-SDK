/*
  File: output_report.h
  Author: Herbert Yeung
  Purpose: Internal DualSense USB output report state and builder declarations.
*/

#ifndef DS5_OUTPUT_REPORT_H
#define DS5_OUTPUT_REPORT_H

#include <stdint.h>

#include <dualsense/dualsense.h>

typedef struct ds5_output_state {
  uint8_t lightbar_r;
  uint8_t lightbar_g;
  uint8_t lightbar_b;
  uint8_t player_leds;
  ds5_mic_led mic_led;
  uint8_t left_rumble;
  uint8_t right_rumble;
  ds5_trigger_effect left_trigger;
  ds5_trigger_effect right_trigger;
} ds5_output_state;

typedef struct ds5_internal_output_report {
  uint8_t bytes[78];
  uint32_t size;
} ds5_internal_output_report;

ds5_internal_output_report ds5_internal_build_usb_output_report(const ds5_output_state* state);

#endif
