#include <dualsense/dualsense.hpp>

#include <iostream>

int main() {
  try {
    DualSense::Context context;
    const auto devices = context.enumerate();
    std::cout << "DualSense SDK " << DualSense::runtimeVersionString() << "\n";
    std::cout << "controllers=" << devices.size() << "\n";
    if (!devices.empty()) {
      DualSense::Controller controller(context.native(), devices.front());
      const auto caps = controller.capabilities();
      std::cout << "capabilities=0x" << std::hex << caps.flags << std::dec << "\n";
    }
  } catch (const DualSense::Error& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
  return 0;
}
