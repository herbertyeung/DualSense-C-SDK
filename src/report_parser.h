/*
  File: report_parser.h
  Author: Herbert Yeung
  Purpose: Internal DualSense HID input report parser declarations.
*/

#ifndef DS5_REPORT_PARSER_H
#define DS5_REPORT_PARSER_H

#include <stddef.h>
#include <stdint.h>

#include <dualsense/dualsense.h>

ds5_result ds5_internal_parse_input_report(const uint8_t* bytes, size_t size, ds5_transport transport, ds5_state* state);
ds5_capabilities ds5_internal_capabilities_for_transport(ds5_transport transport);

#endif
