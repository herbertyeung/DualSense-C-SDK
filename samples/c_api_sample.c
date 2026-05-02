#include <dualsense/dualsense.h>

#include <stdio.h>
#include <stdlib.h>

int main(void) {
  ds5_context* context = NULL;
  if (ds5_init(&context) != DS5_OK) {
    puts(ds5_get_last_error());
    return 1;
  }

  unsigned int count = 0;
  ds5_result result = ds5_enumerate(context, NULL, 0, &count);
  if (result != DS5_OK && result != DS5_E_INSUFFICIENT_BUFFER) {
    puts(ds5_get_last_error());
    ds5_shutdown(context);
    return 1;
  }

  printf("DualSense controllers found: %u\n", count);
  if (count > 0) {
    ds5_device_info* devices = (ds5_device_info*)calloc(count, sizeof(ds5_device_info));
    if (!devices) {
      ds5_shutdown(context);
      return 1;
    }
    for (unsigned int i = 0; i < count; ++i) {
      devices[i].size = sizeof(ds5_device_info);
      devices[i].version = DS5_STRUCT_VERSION;
    }
    result = ds5_enumerate(context, devices, count, &count);
    if (result == DS5_OK) {
      ds5_device* device = NULL;
      result = ds5_open(context, &devices[0], &device);
      if (result == DS5_OK) {
        ds5_capabilities caps;
        ds5_state state;
        caps.size = sizeof(caps);
        caps.version = DS5_STRUCT_VERSION;
        state.size = sizeof(state);
        state.version = DS5_STRUCT_VERSION;
        if (ds5_get_capabilities(device, &caps) == DS5_OK) {
          printf("Capabilities: 0x%08x\n", caps.flags);
        }
        if (ds5_poll_state(device, &state) == DS5_OK) {
          printf("Buttons: 0x%08x L2=%u R2=%u\n", state.buttons, state.left_trigger, state.right_trigger);
        }
        ds5_close(device);
      } else {
        puts(ds5_get_last_error());
      }
    }
    free(devices);
  }
  ds5_shutdown(context);
  return 0;
}
