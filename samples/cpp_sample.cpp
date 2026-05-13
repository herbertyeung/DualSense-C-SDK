#include <dualsense/dualsense.hpp>

#include <iostream>

int main() {
  try {
    DualSense::Context context;
    std::cout << "DualSense SDK " << DualSense::runtimeVersionString() << "\n";
    auto devices = context.enumerate();
    std::cout << "DualSense controllers found: " << devices.size() << "\n";
    if (!devices.empty()) {
      DualSense::Controller controller(context.native(), devices.front());
      const ds5_capabilities caps = controller.capabilities();
      std::cout << "Capabilities: 0x" << std::hex << caps.flags << std::dec << "\n";
      ds5_state state{};
      if (controller.tryState(state)) {
        std::cout << "Buttons: 0x" << std::hex << state.buttons << std::dec
                  << " L2=" << static_cast<int>(state.left_trigger)
                  << " R2=" << static_cast<int>(state.right_trigger) << "\n";
      } else {
        std::cout << "No input report ready immediately\n";
      }
      controller.setLightbar(0, 64, 255);
      controller.setPlayerLeds(0x15);
      controller.setMicLed(DS5_MIC_LED_PULSE);
      controller.haptics().rumble(32, 32);
      controller.triggers().setResistance(true, 16, 128);
      controller.resetFeedback();
    }
  } catch (const DualSense::Error& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
  return 0;
}
