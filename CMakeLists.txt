cmake_minimum_required(VERSION 3.24)

project(dualsense_sdk VERSION 0.2.0 LANGUAGES C CXX)

option(DS5_BUILD_SHARED "Build dualsense shared library" ON)
option(DS5_BUILD_TESTS "Build dualsense tests" ON)
option(DS5_BUILD_SAMPLES "Build dualsense samples" ON)
option(DS5_BUILD_TOOLS "Build dualsense diagnostic tools" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT WIN32)
  message(FATAL_ERROR "dualsense_sdk v1 targets Windows 10/11 only")
endif()

add_library(dualsense_objects OBJECT
  src/dualsense_c_api.cpp
  src/core.cpp
  src/hid_backend_win.cpp
  src/audio_wasapi_win.cpp
  src/report_parser.cpp
  src/output_report.cpp
)
target_include_directories(dualsense_objects
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_definitions(dualsense_objects PRIVATE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

if(MSVC)
  target_compile_options(dualsense_objects PRIVATE /W4 /permissive-)
else()
  target_compile_options(dualsense_objects PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(DS5_BUILD_SHARED)
  add_library(dualsense SHARED $<TARGET_OBJECTS:dualsense_objects>)
  set_target_properties(dualsense PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
  target_include_directories(dualsense
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
  )
  target_compile_definitions(dualsense PRIVATE DS5_BUILDING_DLL UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_compile_definitions(dualsense INTERFACE DS5_USE_DLL)
  target_link_libraries(dualsense PRIVATE hid setupapi ole32 uuid)
endif()

add_library(dualsense_static STATIC $<TARGET_OBJECTS:dualsense_objects>)
target_include_directories(dualsense_static
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(dualsense_static PRIVATE hid setupapi ole32 uuid)

if(DS5_BUILD_TOOLS)
  add_executable(dualsense_diag tools/dualsense_diag.cpp tools/wav_pcm.cpp)
  target_link_libraries(dualsense_diag PRIVATE dualsense_static)

  add_executable(dualsense_ship_demo WIN32 tools/dualsense_ship_demo.cpp tools/glb_loader.cpp tools/wav_pcm.cpp)
  target_compile_definitions(dualsense_ship_demo PRIVATE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_link_libraries(dualsense_ship_demo PRIVATE dualsense_static d3d11 dxgi d3dcompiler xinput)
endif()

if(DS5_BUILD_SAMPLES)
  add_executable(c_api_sample samples/c_api_sample.c)
  target_link_libraries(c_api_sample PRIVATE dualsense_static)

  add_executable(cpp_sample samples/cpp_sample.cpp)
  target_link_libraries(cpp_sample PRIVATE dualsense_static)
endif()

if(DS5_BUILD_TESTS)
  enable_testing()
  add_executable(dualsense_tests
    tests/test_main.cpp
    tools/wav_pcm.cpp
  )
  target_include_directories(dualsense_tests PRIVATE src include)
  target_link_libraries(dualsense_tests PRIVATE dualsense_static)
  add_test(NAME dualsense_tests COMMAND dualsense_tests)
  set_tests_properties(dualsense_tests PROPERTIES
    LABELS "unit;sdk;parser;output;demo"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
endif()

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set(DS5_CMAKE_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/dualsense)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS dualsense_static
  EXPORT dualsenseTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
if(DS5_BUILD_SHARED)
  install(TARGETS dualsense
    EXPORT dualsenseTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )
endif()
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(DIRECTORY samples/ DESTINATION ${CMAKE_INSTALL_DATADIR}/dualsense/samples)
install(FILES README.md CHANGELOG.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(EXPORT dualsenseTargets
  FILE DualSenseTargets.cmake
  NAMESPACE DualSense::
  DESTINATION ${DS5_CMAKE_INSTALL_DIR}
)

configure_package_config_file(
  cmake/dualsenseConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/DualSenseConfig.cmake
  INSTALL_DESTINATION ${DS5_CMAKE_INSTALL_DIR}
)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/DualSenseConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMinorVersion
)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/DualSenseConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/DualSenseConfigVersion.cmake
  DESTINATION ${DS5_CMAKE_INSTALL_DIR}
)
