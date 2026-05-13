cmake_minimum_required(VERSION 3.24)

project(dualsense_consumer LANGUAGES C CXX)

find_package(DualSense CONFIG REQUIRED)

add_executable(dualsense_consumer main.cpp)
target_compile_features(dualsense_consumer PRIVATE cxx_std_17)

if(TARGET DualSense::dualsense_static)
  target_link_libraries(dualsense_consumer PRIVATE DualSense::dualsense_static)
elseif(TARGET DualSense::dualsense)
  target_link_libraries(dualsense_consumer PRIVATE DualSense::dualsense)
else()
  message(FATAL_ERROR "DualSense package did not provide a usable library target")
endif()
