#include <dualsense/dualsense.h>

/*
  The exported C API is implemented in the subsystem files:
  - core.cpp: context, errors, capabilities, close
  - hid_backend_win.cpp: device enumeration, open, input, HID output
  - audio_wasapi_win.cpp: Windows audio endpoints, playback, capture

  This translation unit intentionally exists so downstream build systems have
  a stable C API source boundary to extend without changing CMake target shape.
*/
