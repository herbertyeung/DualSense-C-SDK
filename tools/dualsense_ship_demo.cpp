#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <Xinput.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <dualsense/dualsense.hpp>

#include "ship_controls.h"
#include "ship_feedback.h"
#include "glb_loader.h"
#include "ship_systems.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace {

struct Vertex {
  XMFLOAT3 position;
  XMFLOAT3 normal;
  XMFLOAT2 uv;
  XMFLOAT3 color;
};

struct Constants {
  XMMATRIX mvp;
  XMMATRIX model;
};

struct DemoApp {
  HINSTANCE instance = nullptr;
  HWND hwnd = nullptr;
  UINT width = 1280;
  UINT height = 720;
  bool running = true;

  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11DeviceContext> context;
  ComPtr<IDXGISwapChain> swap_chain;
  ComPtr<ID3D11RenderTargetView> render_target;
  ComPtr<ID3D11Texture2D> depth_texture;
  ComPtr<ID3D11DepthStencilView> depth_view;
  ComPtr<ID3D11VertexShader> vertex_shader;
  ComPtr<ID3D11PixelShader> pixel_shader;
  ComPtr<ID3D11InputLayout> input_layout;
  ComPtr<ID3D11Buffer> ship_vertices;
  ComPtr<ID3D11Buffer> ship_indices;
  ComPtr<ID3D11Buffer> grid_vertices;
  ComPtr<ID3D11Buffer> constants;
  UINT ship_index_count = 0;
  UINT grid_vertex_count = 0;

  DualSense::Context ds_context;
  DualSense::Controller controller;
  bool has_controller = false;
  bool has_xinput = false;
  XINPUT_STATE previous_xinput{};
  ds5_state previous_ds5{};
  ShipPose pose;
  ShipCameraState camera;
  ShipControlConfig config;
  ShipMotionControl motion;
  ShipDebugState debug;
  SpaceShipInputFrame last_frame;
  ShipInput last_motion_input;
  ds5_state last_ds5{};
  bool has_last_ds5 = false;
  std::vector<ShipTarget> targets;
  std::vector<ShipBullet> bullets;
  std::vector<ShipFeedbackZone> feedback_zones;
  ShipFeedbackZoneKind active_zone = ShipFeedbackZoneKind::None;
  int locked_target = -1;
  bool auto_follow = false;
  bool tactical_panel = false;
  bool paused = false;
  bool weapon_charging = false;
  bool was_firing = false;
  ShipTriggerMode trigger_mode = ShipTriggerMode::Flight;
  std::string speaker_endpoint_id;
  float health = 1.0f;
  float event_timer = 0.0f;
  std::string last_event = "none";
  float feedback_timer = 0.0f;
  float frame_dt = 1.0f / 60.0f;
  float fire_cooldown = 0.0f;
  float flame_time = 0.0f;
  ShipFeedback last_feedback{};
  bool has_last_feedback = false;
};

DemoApp* g_app = nullptr;

void check(HRESULT hr, const char* message) {
  if (FAILED(hr)) {
    throw std::runtime_error(message);
  }
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_DESTROY:
      if (g_app) g_app->running = false;
      PostQuitMessage(0);
      return 0;
    case WM_KEYDOWN:
      if (wparam == VK_ESCAPE) {
        DestroyWindow(hwnd);
        return 0;
      }
      break;
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

ComPtr<ID3DBlob> compile_shader(const char* source, const char* entry, const char* target) {
  UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
  flags |= D3DCOMPILE_DEBUG;
#endif
  ComPtr<ID3DBlob> blob;
  ComPtr<ID3DBlob> errors;
  HRESULT hr = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, entry, target, flags, 0, blob.GetAddressOf(), errors.GetAddressOf());
  if (FAILED(hr)) {
    std::string message = "Shader compilation failed";
    if (errors) {
      message += ": ";
      message += static_cast<const char*>(errors->GetBufferPointer());
    }
    throw std::runtime_error(message);
  }
  return blob;
}

template <typename T>
ComPtr<ID3D11Buffer> create_buffer(ID3D11Device* device, const std::vector<T>& data, D3D11_BIND_FLAG bind) {
  D3D11_BUFFER_DESC desc{};
  desc.ByteWidth = static_cast<UINT>(data.size() * sizeof(T));
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = bind;
  D3D11_SUBRESOURCE_DATA initial{};
  initial.pSysMem = data.data();
  ComPtr<ID3D11Buffer> buffer;
  check(device->CreateBuffer(&desc, &initial, buffer.GetAddressOf()), "CreateBuffer failed");
  return buffer;
}

void create_window(DemoApp& app) {
  WNDCLASSW wc{};
  wc.lpfnWndProc = window_proc;
  wc.hInstance = app.instance;
  wc.lpszClassName = L"DualSenseShipDemoWindow";
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassW(&wc);

  RECT rect{0, 0, static_cast<LONG>(app.width), static_cast<LONG>(app.height)};
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
  app.hwnd = CreateWindowW(wc.lpszClassName, L"DualSense DirectX Ship Test", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                           CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
                           nullptr, nullptr, app.instance, nullptr);
  if (!app.hwnd) {
    throw std::runtime_error("CreateWindowW failed");
  }
}

void create_device(DemoApp& app) {
  DXGI_SWAP_CHAIN_DESC scd{};
  scd.BufferCount = 1;
  scd.BufferDesc.Width = app.width;
  scd.BufferDesc.Height = app.height;
  scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  scd.OutputWindow = app.hwnd;
  scd.SampleDesc.Count = 1;
  scd.Windowed = TRUE;
  scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  UINT flags = 0;
#if defined(_DEBUG)
  flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
  D3D_FEATURE_LEVEL level{};
  HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0,
                                             D3D11_SDK_VERSION, &scd, app.swap_chain.GetAddressOf(),
                                             app.device.GetAddressOf(), &level, app.context.GetAddressOf());
  if (FAILED(hr) && (flags & D3D11_CREATE_DEVICE_DEBUG)) {
    hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                       D3D11_SDK_VERSION, &scd, app.swap_chain.GetAddressOf(),
                                       app.device.GetAddressOf(), &level, app.context.GetAddressOf());
  }
  check(hr, "D3D11CreateDeviceAndSwapChain failed");

  ComPtr<ID3D11Texture2D> back_buffer;
  check(app.swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(back_buffer.GetAddressOf())), "GetBuffer failed");
  check(app.device->CreateRenderTargetView(back_buffer.Get(), nullptr, app.render_target.GetAddressOf()), "CreateRenderTargetView failed");

  D3D11_TEXTURE2D_DESC depth_desc{};
  depth_desc.Width = app.width;
  depth_desc.Height = app.height;
  depth_desc.MipLevels = 1;
  depth_desc.ArraySize = 1;
  depth_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  depth_desc.SampleDesc.Count = 1;
  depth_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
  check(app.device->CreateTexture2D(&depth_desc, nullptr, app.depth_texture.GetAddressOf()), "CreateTexture2D depth failed");
  check(app.device->CreateDepthStencilView(app.depth_texture.Get(), nullptr, app.depth_view.GetAddressOf()), "CreateDepthStencilView failed");

  D3D11_VIEWPORT viewport{};
  viewport.Width = static_cast<float>(app.width);
  viewport.Height = static_cast<float>(app.height);
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  app.context->RSSetViewports(1, &viewport);
}

std::string executable_directory() {
  char path[MAX_PATH]{};
  const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return {};
  std::string out(path, path + length);
  const size_t slash = out.find_last_of("\\/");
  return slash == std::string::npos ? std::string{} : out.substr(0, slash);
}

std::string find_ship_glb_path() {
  const char* relative_paths[] = {
      "ship_textured_uv_normal.glb",
      "../ship_textured_uv_normal.glb",
      "../../ship_textured_uv_normal.glb",
      "../../../ship_textured_uv_normal.glb",
  };
  for (const char* path : relative_paths) {
    std::ifstream file(path, std::ios::binary);
    if (file) return path;
  }

  const std::string exe_dir = executable_directory();
  if (!exe_dir.empty()) {
    const std::string from_exe = exe_dir + "\\..\\..\\ship_textured_uv_normal.glb";
    std::ifstream file(from_exe, std::ios::binary);
    if (file) return from_exe;
  }
  return {};
}

void create_assets(DemoApp& app) {
  const char* shader = R"(
cbuffer Constants : register(b0) { matrix mvp; matrix model; };
struct VSIn { float3 pos : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD; float3 color : COLOR; };
struct PSIn { float4 pos : SV_POSITION; float3 color : COLOR; float3 normal : NORMAL; };
PSIn vs_main(VSIn input) {
  PSIn output;
  output.pos = mul(float4(input.pos, 1.0), mvp);
  output.color = input.color;
  output.normal = normalize(mul(float4(input.normal, 0.0), model).xyz);
  return output;
}
float4 ps_main(PSIn input) : SV_TARGET {
  float3 light = normalize(float3(-0.35, 0.65, -0.55));
  float ndl = saturate(dot(normalize(input.normal), light));
  float3 color = input.color * (0.28 + 0.72 * ndl);
  return float4(color, 1.0);
}
)";

  auto vs = compile_shader(shader, "vs_main", "vs_5_0");
  auto ps = compile_shader(shader, "ps_main", "ps_5_0");
  check(app.device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, app.vertex_shader.GetAddressOf()), "CreateVertexShader failed");
  check(app.device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, app.pixel_shader.GetAddressOf()), "CreatePixelShader failed");

  D3D11_INPUT_ELEMENT_DESC layout[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
  };
  check(app.device->CreateInputLayout(layout, 4, vs->GetBufferPointer(), vs->GetBufferSize(), app.input_layout.GetAddressOf()), "CreateInputLayout failed");

  GlbMeshData glb_ship;
  std::string glb_error;
  const std::string ship_glb_path = find_ship_glb_path();
  if (ship_glb_path.empty() || !ds5_demo_load_glb_mesh(ship_glb_path.c_str(), &glb_ship, &glb_error)) {
    throw std::runtime_error("Required GLB ship failed to load: " + (ship_glb_path.empty() ? std::string("ship_textured_uv_normal.glb not found") : glb_error));
  }
  const float sx = glb_ship.bounds_max.x - glb_ship.bounds_min.x;
  const float sy = glb_ship.bounds_max.y - glb_ship.bounds_min.y;
  const float sz = glb_ship.bounds_max.z - glb_ship.bounds_min.z;
  const float scale = 3.4f / std::max(0.001f, std::max(sx, std::max(sy, sz)));
  const XMFLOAT3 center{
      (glb_ship.bounds_min.x + glb_ship.bounds_max.x) * 0.5f,
      (glb_ship.bounds_min.y + glb_ship.bounds_max.y) * 0.5f,
      (glb_ship.bounds_min.z + glb_ship.bounds_max.z) * 0.5f};
  for (auto& vertex : glb_ship.vertices) {
    vertex.position.x = (vertex.position.x - center.x) * scale;
    vertex.position.y = (vertex.position.y - center.y) * scale;
    vertex.position.z = (vertex.position.z - center.z) * scale;
  }
  app.ship_vertices = create_buffer(app.device.Get(), glb_ship.vertices, D3D11_BIND_VERTEX_BUFFER);
  app.ship_indices = create_buffer(app.device.Get(), glb_ship.indices, D3D11_BIND_INDEX_BUFFER);
  app.ship_index_count = static_cast<UINT>(glb_ship.indices.size());

  std::vector<Vertex> grid;
  for (int i = -20; i <= 20; ++i) {
    float c = (i == 0) ? 0.45f : 0.18f;
    grid.push_back({{-20.0f, -2.0f, static_cast<float>(i)}, {0, 1, 0}, {0, 0}, {c, c, c}});
    grid.push_back({{20.0f, -2.0f, static_cast<float>(i)}, {0, 1, 0}, {0, 0}, {c, c, c}});
    grid.push_back({{static_cast<float>(i), -2.0f, -20.0f}, {0, 1, 0}, {0, 0}, {c, c, c}});
    grid.push_back({{static_cast<float>(i), -2.0f, 20.0f}, {0, 1, 0}, {0, 0}, {c, c, c}});
  }
  app.grid_vertices = create_buffer(app.device.Get(), grid, D3D11_BIND_VERTEX_BUFFER);
  app.grid_vertex_count = static_cast<UINT>(grid.size());

  D3D11_BUFFER_DESC cb{};
  cb.ByteWidth = sizeof(Constants);
  cb.Usage = D3D11_USAGE_DEFAULT;
  cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  check(app.device->CreateBuffer(&cb, nullptr, app.constants.GetAddressOf()), "Create constant buffer failed");
}

float normalize_xinput_thumb(SHORT value, SHORT deadzone) {
  if (std::abs(value) < deadzone) return 0.0f;
  const float denom = value < 0 ? 32768.0f : 32767.0f;
  return ds5_demo_clamp(static_cast<float>(value) / denom, -1.0f, 1.0f);
}

bool pressed_now(WORD buttons, WORD previous, WORD mask) {
  return (buttons & mask) != 0 && (previous & mask) == 0;
}

bool pressed_now_ds5(const ds5_state& state, const ds5_state& previous, uint32_t mask) {
  return (state.buttons & mask) != 0u && (previous.buttons & mask) == 0u;
}

float flight_pitch_from_camera(const ShipCameraState& camera) {
  return ds5_demo_clamp(camera.pitch - 0.2f, -0.95f, 0.95f);
}

ShipPose ship_head_pose(const DemoApp& app) {
  ShipPose aim = app.pose;
  aim.yaw += app.camera.yaw;
  aim.pitch = flight_pitch_from_camera(app.camera);
  return aim;
}

ShipInput keyboard_input() {
  ShipInput input{};
  if (GetAsyncKeyState(VK_LEFT) & 0x8000) input.yaw -= 1.0f;
  if (GetAsyncKeyState(VK_RIGHT) & 0x8000) input.yaw += 1.0f;
  if (GetAsyncKeyState(VK_UP) & 0x8000) input.pitch += 1.0f;
  if (GetAsyncKeyState(VK_DOWN) & 0x8000) input.pitch -= 1.0f;
  if (GetAsyncKeyState('A') & 0x8000) input.roll -= 1.0f;
  if (GetAsyncKeyState('D') & 0x8000) input.roll += 1.0f;
  if (GetAsyncKeyState(VK_SPACE) & 0x8000) input.throttle = 1.0f;
  if (GetAsyncKeyState(VK_SHIFT) & 0x8000) input.brake = 1.0f;
  return input;
}

SpaceShipInputFrame keyboard_frame() {
  SpaceShipInputFrame frame{};
  frame.flight = keyboard_input();
  if (GetAsyncKeyState('J') & 0x8000) frame.cameraYaw -= 1.0f;
  if (GetAsyncKeyState('L') & 0x8000) frame.cameraYaw += 1.0f;
  if (GetAsyncKeyState('I') & 0x8000) frame.cameraPitch += 1.0f;
  if (GetAsyncKeyState('K') & 0x8000) frame.cameraPitch -= 1.0f;
  if (GetAsyncKeyState('Q') & 0x8000) frame.flight.roll -= 1.0f;
  if (GetAsyncKeyState('E') & 0x8000) frame.flight.roll += 1.0f;
  frame.fire = (GetAsyncKeyState('F') & 0x8000) != 0;
  frame.boost = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
  frame.dodge = (GetAsyncKeyState('C') & 0x0001) != 0;
  frame.switchWeapon = (GetAsyncKeyState('Z') & 0x0001) != 0;
  frame.lockTarget = (GetAsyncKeyState('T') & 0x0001) != 0;
  frame.toggleAutoFollow = (GetAsyncKeyState(VK_TAB) & 0x0001) != 0;
  frame.pause = (GetAsyncKeyState('P') & 0x0001) != 0;
  return frame;
}

SpaceShipInputFrame xinput_frame(DemoApp& app) {
  SpaceShipInputFrame frame{};
  XINPUT_STATE state{};
  if (XInputGetState(0, &state) != ERROR_SUCCESS) {
    app.has_xinput = false;
    return frame;
  }
  app.has_xinput = true;
  const WORD buttons = state.Gamepad.wButtons;
  const WORD previous = app.previous_xinput.Gamepad.wButtons;
  frame.flight.strafe = ds5_demo_apply_deadzone(normalize_xinput_thumb(state.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE), app.config.stickDeadzone);
  frame.flight.moveForward = ds5_demo_apply_deadzone(normalize_xinput_thumb(state.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE), app.config.stickDeadzone);
  frame.cameraYaw = ds5_demo_apply_deadzone(normalize_xinput_thumb(state.Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE), app.config.stickDeadzone);
  frame.cameraPitch = ds5_demo_apply_deadzone(normalize_xinput_thumb(state.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE), app.config.stickDeadzone);
  frame.flight.roll = ((buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 1.0f : 0.0f) - ((buttons & XINPUT_GAMEPAD_LEFT_SHOULDER) ? 1.0f : 0.0f);
  frame.flight.brake = std::max(frame.flight.brake, static_cast<float>(state.Gamepad.bLeftTrigger) / 255.0f);
  frame.fire = ds5_demo_r2_fires_light_bullet(static_cast<float>(state.Gamepad.bRightTrigger) / 255.0f);
  frame.boost = (buttons & XINPUT_GAMEPAD_A) != 0;
  frame.dodge = pressed_now(buttons, previous, XINPUT_GAMEPAD_B);
  frame.switchWeapon = pressed_now(buttons, previous, XINPUT_GAMEPAD_X);
  frame.lockTarget = pressed_now(buttons, previous, XINPUT_GAMEPAD_Y);
  frame.toggleAutoFollow = pressed_now(buttons, previous, XINPUT_GAMEPAD_BACK);
  frame.pause = pressed_now(buttons, previous, XINPUT_GAMEPAD_START);
  app.previous_xinput = state;
  return frame;
}

SpaceShipInputFrame dualsense_frame(DemoApp& app) {
  SpaceShipInputFrame frame{};
  if (!app.has_controller) return frame;
  try {
    ds5_state state = app.controller.state();
    frame.flight = ds5_demo_input_from_state(state);
    frame.flight.strafe = ds5_demo_apply_deadzone(frame.flight.strafe, app.config.stickDeadzone);
    frame.flight.moveForward = ds5_demo_apply_deadzone(frame.flight.moveForward, app.config.stickDeadzone);
    frame.flight.pitch = ds5_demo_apply_deadzone(frame.flight.pitch, app.config.stickDeadzone);
    frame.cameraYaw = ds5_demo_apply_deadzone(ds5_demo_axis_from_byte(state.right_stick_x), app.config.stickDeadzone);
    frame.cameraPitch = ds5_demo_apply_deadzone(ds5_demo_axis_from_byte(state.right_stick_y, true), app.config.stickDeadzone);
    if (state.buttons & DS5_BUTTON_L1) frame.flight.roll -= 1.0f;
    if (state.buttons & DS5_BUTTON_R1) frame.flight.roll += 1.0f;
    ShipInput gyro_attitude = app.motion.updateAttitude(state, app.config, app.frame_dt);
    app.motion.writeDebug(app.debug);
    app.last_motion_input = gyro_attitude;
    frame.flight.pitch = ds5_demo_clamp(frame.flight.pitch + gyro_attitude.pitch, -1.0f, 1.0f);
    frame.flight.yaw = ds5_demo_clamp(frame.flight.yaw + gyro_attitude.yaw, -1.0f, 1.0f);
    frame.flight.roll = ds5_demo_clamp(frame.flight.roll + gyro_attitude.roll, -1.0f, 1.0f);
    frame.boost = (state.buttons & DS5_BUTTON_CROSS) != 0u;
    frame.dodge = pressed_now_ds5(state, app.previous_ds5, DS5_BUTTON_CIRCLE) || app.motion.detectDodge(state, app.config);
    frame.switchWeapon = pressed_now_ds5(state, app.previous_ds5, DS5_BUTTON_SQUARE);
    frame.lockTarget = pressed_now_ds5(state, app.previous_ds5, DS5_BUTTON_TRIANGLE);
    frame.toggleAutoFollow = pressed_now_ds5(state, app.previous_ds5, DS5_BUTTON_TOUCHPAD);
    frame.touchpad = frame.toggleAutoFollow;
    frame.pause = pressed_now_ds5(state, app.previous_ds5, DS5_BUTTON_OPTIONS);
    frame.fire = ds5_demo_r2_fires_light_bullet(static_cast<float>(state.right_trigger) / 255.0f);
    app.last_ds5 = state;
    app.has_last_ds5 = true;
    app.previous_ds5 = state;
    return frame;
  } catch (...) {
    app.has_controller = false;
    return frame;
  }
}

SpaceShipInputFrame merged_input_frame(DemoApp& app) {
  SpaceShipInputFrame out = keyboard_frame();
  SpaceShipInputFrame xi = xinput_frame(app);
  SpaceShipInputFrame ds = dualsense_frame(app);
  auto merge_axis = [](float a, float b) { return std::fabs(b) > std::fabs(a) ? b : a; };
  for (const SpaceShipInputFrame* source : {&xi, &ds}) {
    out.flight.pitch = merge_axis(out.flight.pitch, source->flight.pitch);
    out.flight.yaw = merge_axis(out.flight.yaw, source->flight.yaw);
    out.flight.roll = merge_axis(out.flight.roll, source->flight.roll);
    out.flight.strafe = merge_axis(out.flight.strafe, source->flight.strafe);
    out.flight.moveForward = merge_axis(out.flight.moveForward, source->flight.moveForward);
    out.flight.throttle = std::max(out.flight.throttle, source->flight.throttle);
    out.flight.brake = std::max(out.flight.brake, source->flight.brake);
    out.cameraYaw = merge_axis(out.cameraYaw, source->cameraYaw);
    out.cameraPitch = merge_axis(out.cameraPitch, source->cameraPitch);
    out.boost = out.boost || source->boost;
    out.dodge = out.dodge || source->dodge;
    out.fire = out.fire || source->fire;
    out.switchWeapon = out.switchWeapon || source->switchWeapon;
    out.lockTarget = out.lockTarget || source->lockTarget;
    out.toggleAutoFollow = out.toggleAutoFollow || source->toggleAutoFollow;
    out.pause = out.pause || source->pause;
    out.touchpad = out.touchpad || source->touchpad;
  }
  return out;
}

void draw_debug_overlay(DemoApp& app) {
  HDC dc = GetDC(app.hwnd);
  if (!dc) return;
  RECT rect{10, 10, 560, 250};
  HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));
  FillRect(dc, &rect, brush);
  DeleteObject(brush);
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, RGB(185, 235, 255));

  std::ostringstream text;
  text.setf(std::ios::fixed);
  text.precision(3);
  text << "Input: " << (app.has_controller ? "DualSense USB" : (app.has_xinput ? "XInput" : "Keyboard")) << "\n";
    if (app.has_last_ds5) {
    const ds5_state& s = app.last_ds5;
    text << "DS5 buttons=0x" << std::hex << s.buttons << std::dec
         << " LS=(" << static_cast<int>(s.left_stick_x) << "," << static_cast<int>(s.left_stick_y) << ")"
         << " RS=(" << static_cast<int>(s.right_stick_x) << "," << static_cast<int>(s.right_stick_y) << ")"
         << " L2/R2=" << static_cast<int>(s.left_trigger) << "/" << static_cast<int>(s.right_trigger) << "\n";
    text << "gyro=(" << s.gyro_x << "," << s.gyro_y << "," << s.gyro_z << ")"
         << " accel=(" << s.accel_x << "," << s.accel_y << "," << s.accel_z << ")\n";
    text << "gyro bias=(" << app.debug.gyroBiasX << "," << app.debug.gyroBiasY << "," << app.debug.gyroBiasZ << ")"
         << " steady=" << (app.debug.gyroSteady ? "yes" : "no")
         << " confidence=" << app.debug.gyroCalibrationConfidence << "\n";
  } else {
    text << "DS5 not active; using fallback input if available\n";
  }
  text << "motion pitch/yaw/roll="
       << app.last_motion_input.pitch << " / " << app.last_motion_input.yaw << " / " << app.last_motion_input.roll
       << " deadzone=" << app.config.motionInputDeadzone << "\n";
  text << "flight strafe/forward/throttle/brake/yaw/pitch/roll="
       << app.last_frame.flight.strafe << " / " << app.last_frame.flight.moveForward
       << " / " << app.last_frame.flight.throttle << " / " << app.last_frame.flight.brake
       << " / " << app.last_frame.flight.yaw << " / " << app.last_frame.flight.pitch
       << " / " << app.last_frame.flight.roll << "\n";
  text << "camera yaw/pitch input=" << app.last_frame.cameraYaw << " / " << app.last_frame.cameraPitch
       << " camera state=" << app.camera.yaw << " / " << app.camera.pitch << "\n";
  text << "ship speed=" << app.pose.speed << " yaw=" << app.pose.yaw << " pitch=" << app.pose.pitch
       << " roll=" << app.pose.roll << " bullets=" << app.bullets.size()
       << " zone=" << ds5_demo_feedback_zone_name(app.active_zone)
       << " event=" << app.last_event << "\n";

  DrawTextA(dc, text.str().c_str(), -1, &rect, DT_LEFT | DT_TOP | DT_NOPREFIX);
  ReleaseDC(app.hwnd, dc);
}

void reset_controller_feedback(DemoApp& app) {
  if (!app.has_controller) {
    return;
  }
  try {
    app.controller.resetFeedback();
  } catch (...) {
  }
}

class DualSenseFeedbackManager {
 public:
  void update(DemoApp& app, const ShipInput& input, float dt) {
    if (!app.has_controller) return;
    app.feedback_timer += dt;
    if (app.feedback_timer < 0.08f) return;
    app.feedback_timer = 0.0f;

    const bool low_health = app.health < 0.28f;
    ShipFeedback feedback = ds5_demo_feedback_from_ship(app.pose, input, app.trigger_mode, app.weapon_charging, low_health);
    ds5_demo_apply_zone_feedback(feedback, app.active_zone);
    if (app.last_frame.fire) {
      feedback.right_trigger = ds5_demo_r2_auto_fire_trigger_effect(app.active_zone);
      feedback.right_rumble = app.active_zone == ShipFeedbackZoneKind::None ? 22 : 52;
      feedback.event_name = app.active_zone == ShipFeedbackZoneKind::None ? "auto fire" : ds5_demo_feedback_zone_name(app.active_zone);
    }
    feedback.left_rumble = static_cast<uint8_t>(feedback.left_rumble * app.config.vibrationIntensityScale);
    feedback.right_rumble = static_cast<uint8_t>(feedback.right_rumble * app.config.vibrationIntensityScale);
    if (!app.last_frame.fire) {
      feedback.right_rumble = 0;
    }

    auto changed = [](uint8_t a, uint8_t b, uint8_t threshold) {
      return a > b ? (a - b) >= threshold : (b - a) >= threshold;
    };
    const bool needs_write =
        !app.has_last_feedback ||
        feedback.right_trigger.mode != app.last_feedback.right_trigger.mode ||
        feedback.right_trigger.start_position != app.last_feedback.right_trigger.start_position ||
        feedback.right_trigger.end_position != app.last_feedback.right_trigger.end_position ||
        feedback.right_trigger.frequency != app.last_feedback.right_trigger.frequency ||
        feedback.left_trigger.mode != app.last_feedback.left_trigger.mode ||
        feedback.left_trigger.start_position != app.last_feedback.left_trigger.start_position ||
        feedback.left_trigger.end_position != app.last_feedback.left_trigger.end_position ||
        feedback.left_trigger.frequency != app.last_feedback.left_trigger.frequency ||
        changed(feedback.right_trigger.force, app.last_feedback.right_trigger.force, 4) ||
        changed(feedback.left_trigger.force, app.last_feedback.left_trigger.force, 5) ||
        changed(feedback.left_rumble, app.last_feedback.left_rumble, 8) ||
        changed(feedback.right_rumble, app.last_feedback.right_rumble, 8) ||
        changed(feedback.light_r, app.last_feedback.light_r, 20) ||
        changed(feedback.light_g, app.last_feedback.light_g, 20) ||
        changed(feedback.light_b, app.last_feedback.light_b, 20);
    if (!needs_write) return;

    try {
      if (app.config.adaptiveTriggerEnabled) {
        app.controller.triggers().setEffect(false, feedback.right_trigger);
        app.controller.triggers().setEffect(true, feedback.left_trigger);
      }
      app.controller.haptics().rumble(feedback.left_rumble, feedback.right_rumble);
      app.controller.setLightbar(feedback.light_r, feedback.light_g, feedback.light_b);
      app.last_feedback = feedback;
      app.has_last_feedback = true;
      app.last_event = feedback.event_name;
    } catch (...) {
      app.has_controller = false;
    }
  }
};

void draw_vertices(DemoApp& app, ID3D11Buffer* vertices, UINT vertex_count, D3D11_PRIMITIVE_TOPOLOGY topology, XMMATRIX mvp, XMMATRIX model) {
  UINT stride = sizeof(Vertex);
  UINT offset = 0;
  app.context->IASetVertexBuffers(0, 1, &vertices, &stride, &offset);
  app.context->IASetPrimitiveTopology(topology);
  Constants constants{XMMatrixTranspose(mvp), XMMatrixTranspose(model)};
  app.context->UpdateSubresource(app.constants.Get(), 0, nullptr, &constants, 0, 0);
  app.context->Draw(vertex_count, 0);
}

std::vector<Vertex> build_light_bullet_vertices(const std::vector<ShipBullet>& bullets) {
  std::vector<Vertex> vertices;
  vertices.reserve(bullets.size() * 6u);
  for (const ShipBullet& bullet : bullets) {
    const float len = std::max(0.001f, std::sqrt(bullet.vx * bullet.vx + bullet.vy * bullet.vy + bullet.vz * bullet.vz));
    const float dx = bullet.vx / len;
    const float dy = bullet.vy / len;
    const float dz = bullet.vz / len;
    const float trail = 1.35f;
    const float fade = ds5_demo_clamp(1.0f - bullet.age / std::max(0.001f, bullet.lifetime), 0.0f, 1.0f);
    const XMFLOAT3 head{bullet.x, bullet.y, bullet.z};
    const XMFLOAT3 tail{bullet.x - dx * trail, bullet.y - dy * trail, bullet.z - dz * trail};
    const XMFLOAT3 normal{0.0f, 1.0f, 0.0f};
    const XMFLOAT3 core{0.85f + 0.15f * fade, 0.96f, 1.0f};
    const XMFLOAT3 glow{0.10f, 0.55f + 0.30f * fade, 1.0f};
    const float side = 0.055f;
    vertices.push_back({tail, normal, {0, 0}, core});
    vertices.push_back({head, normal, {0, 0}, core});
    vertices.push_back({{tail.x - side, tail.y, tail.z}, normal, {0, 0}, glow});
    vertices.push_back({{head.x - side, head.y, head.z}, normal, {0, 0}, glow});
    vertices.push_back({{tail.x + side, tail.y, tail.z}, normal, {0, 0}, glow});
    vertices.push_back({{head.x + side, head.y, head.z}, normal, {0, 0}, glow});
  }
  return vertices;
}

std::vector<Vertex> build_engine_flame_vertices(const ShipPose& head_pose, float intensity, float time) {
  std::vector<Vertex> vertices;
  if (intensity <= 0.02f) return vertices;

  const float cp = std::cos(head_pose.pitch);
  const float fx = std::sin(head_pose.yaw) * cp;
  const float fy = -std::sin(head_pose.pitch);
  const float fz = std::cos(head_pose.yaw) * cp;
  const float rx = std::cos(head_pose.yaw);
  const float rz = -std::sin(head_pose.yaw);
  const float flicker = 0.82f + 0.18f * std::sin(time * 42.0f);
  const float core_len = (0.85f + 1.25f * intensity) * flicker;
  const float glow_len = core_len * 1.35f;
  const XMFLOAT3 normal{0.0f, 1.0f, 0.0f};
  const XMFLOAT3 core{1.0f, 0.52f + 0.25f * intensity, 0.10f};
  const XMFLOAT3 hot{1.0f, 0.93f, 0.62f};
  const XMFLOAT3 blue{0.18f, 0.62f, 1.0f};
  const XMFLOAT3 engine{
      head_pose.x - fx * 1.72f,
      head_pose.y - fy * 1.72f,
      head_pose.z - fz * 1.72f};
  const XMFLOAT3 core_end{
      engine.x - fx * core_len,
      engine.y - fy * core_len,
      engine.z - fz * core_len};
  const XMFLOAT3 glow_end{
      engine.x - fx * glow_len,
      engine.y - fy * glow_len,
      engine.z - fz * glow_len};
  const float side = 0.12f + 0.08f * intensity;
  const float up = 0.10f;

  vertices.push_back({engine, normal, {0, 0}, hot});
  vertices.push_back({core_end, normal, {0, 0}, core});
  vertices.push_back({{engine.x - rx * side, engine.y, engine.z - rz * side}, normal, {0, 0}, blue});
  vertices.push_back({{glow_end.x - rx * side * 0.45f, glow_end.y, glow_end.z - rz * side * 0.45f}, normal, {0, 0}, blue});
  vertices.push_back({{engine.x + rx * side, engine.y, engine.z + rz * side}, normal, {0, 0}, blue});
  vertices.push_back({{glow_end.x + rx * side * 0.45f, glow_end.y, glow_end.z + rz * side * 0.45f}, normal, {0, 0}, blue});
  vertices.push_back({{engine.x, engine.y + up, engine.z}, normal, {0, 0}, core});
  vertices.push_back({{glow_end.x, glow_end.y + up * 0.25f, glow_end.z}, normal, {0, 0}, core});
  vertices.push_back({{engine.x, engine.y - up, engine.z}, normal, {0, 0}, core});
  vertices.push_back({{glow_end.x, glow_end.y - up * 0.25f, glow_end.z}, normal, {0, 0}, core});
  return vertices;
}

XMFLOAT3 feedback_zone_color(ShipFeedbackZoneKind kind) {
  if (kind == ShipFeedbackZoneKind::Pulse) return {0.15f, 0.45f, 1.0f};
  if (kind == ShipFeedbackZoneKind::Heavy) return {1.0f, 0.12f, 0.08f};
  if (kind == ShipFeedbackZoneKind::Rapid) return {0.15f, 1.0f, 0.35f};
  if (kind == ShipFeedbackZoneKind::Sticky) return {0.72f, 0.25f, 1.0f};
  return {0.55f, 0.55f, 0.55f};
}

void add_line(std::vector<Vertex>& vertices, const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& color) {
  const XMFLOAT3 normal{0.0f, 1.0f, 0.0f};
  vertices.push_back({a, normal, {0, 0}, color});
  vertices.push_back({b, normal, {0, 0}, color});
}

std::vector<Vertex> build_feedback_zone_vertices(const std::vector<ShipFeedbackZone>& zones, ShipFeedbackZoneKind active_zone) {
  std::vector<Vertex> vertices;
  vertices.reserve(zones.size() * 24u);
  for (const ShipFeedbackZone& zone : zones) {
    XMFLOAT3 color = feedback_zone_color(zone.kind);
    if (zone.kind == active_zone) {
      color.x = std::min(1.0f, color.x + 0.25f);
      color.y = std::min(1.0f, color.y + 0.25f);
      color.z = std::min(1.0f, color.z + 0.25f);
    }
    const float e = zone.half_extent;
    const float x0 = zone.x - e;
    const float x1 = zone.x + e;
    const float y0 = zone.y - e;
    const float y1 = zone.y + e;
    const float z0 = zone.z - e;
    const float z1 = zone.z + e;
    const XMFLOAT3 p000{x0, y0, z0};
    const XMFLOAT3 p001{x0, y0, z1};
    const XMFLOAT3 p010{x0, y1, z0};
    const XMFLOAT3 p011{x0, y1, z1};
    const XMFLOAT3 p100{x1, y0, z0};
    const XMFLOAT3 p101{x1, y0, z1};
    const XMFLOAT3 p110{x1, y1, z0};
    const XMFLOAT3 p111{x1, y1, z1};
    add_line(vertices, p000, p001, color);
    add_line(vertices, p001, p101, color);
    add_line(vertices, p101, p100, color);
    add_line(vertices, p100, p000, color);
    add_line(vertices, p010, p011, color);
    add_line(vertices, p011, p111, color);
    add_line(vertices, p111, p110, color);
    add_line(vertices, p110, p010, color);
    add_line(vertices, p000, p010, color);
    add_line(vertices, p001, p011, color);
    add_line(vertices, p100, p110, color);
    add_line(vertices, p101, p111, color);
  }
  return vertices;
}

void render(DemoApp& app) {
  float clear[4] = {0.015f, 0.02f, 0.035f, 1.0f};
  app.context->ClearRenderTargetView(app.render_target.Get(), clear);
  app.context->ClearDepthStencilView(app.depth_view.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
  ID3D11RenderTargetView* rtv = app.render_target.Get();
  app.context->OMSetRenderTargets(1, &rtv, app.depth_view.Get());
  app.context->IASetInputLayout(app.input_layout.Get());
  app.context->VSSetShader(app.vertex_shader.Get(), nullptr, 0);
  app.context->PSSetShader(app.pixel_shader.Get(), nullptr, 0);
  ID3D11Buffer* cb = app.constants.Get();
  app.context->VSSetConstantBuffers(0, 1, &cb);

  const float camera_yaw = app.pose.yaw + app.camera.yaw;
  const float camera_x = app.camera.hasFollowTarget ? app.camera.followX : app.pose.x;
  const float camera_y = app.camera.hasFollowTarget ? app.camera.followY : app.pose.y;
  const float camera_z = app.camera.hasFollowTarget ? app.camera.followZ : app.pose.z;
  XMVECTOR eye = XMVectorSet(camera_x - std::sin(camera_yaw) * 8.0f, camera_y + 4.0f + std::sin(app.camera.pitch) * 2.0f,
                             camera_z - std::cos(camera_yaw) * 8.0f, 1.0f);
  XMVECTOR at = XMVectorSet(camera_x, camera_y, camera_z, 1.0f);
  XMMATRIX view = XMMatrixLookAtLH(eye, at, XMVectorSet(0, 1, 0, 0));
  XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, static_cast<float>(app.width) / static_cast<float>(app.height), 0.1f, 100.0f);

  const float visual_pitch = app.pose.pitch - app.last_frame.flight.moveForward * 0.18f;
  const float view_flight_pitch = flight_pitch_from_camera(app.camera);
  const float visual_roll = app.pose.roll - app.last_frame.flight.strafe * 0.28f;
  XMMATRIX ship_model =
      XMMatrixRotationRollPitchYaw(visual_pitch + view_flight_pitch, app.pose.yaw + app.camera.yaw, visual_roll) *
      XMMatrixTranslation(app.pose.x, app.pose.y, app.pose.z);
  XMMATRIX grid_model = XMMatrixIdentity();

  draw_vertices(app, app.grid_vertices.Get(), app.grid_vertex_count, D3D11_PRIMITIVE_TOPOLOGY_LINELIST, grid_model * view * proj, grid_model);

  std::vector<Vertex> zone_vertices = build_feedback_zone_vertices(app.feedback_zones, app.active_zone);
  if (!zone_vertices.empty()) {
    ComPtr<ID3D11Buffer> zone_buffer = create_buffer(app.device.Get(), zone_vertices, D3D11_BIND_VERTEX_BUFFER);
    XMMATRIX zone_model = XMMatrixIdentity();
    draw_vertices(app, zone_buffer.Get(), static_cast<UINT>(zone_vertices.size()), D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
                  zone_model * view * proj, zone_model);
  }

  UINT stride = sizeof(Vertex);
  UINT offset = 0;
  ID3D11Buffer* vb = app.ship_vertices.Get();
  app.context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
  app.context->IASetIndexBuffer(app.ship_indices.Get(), DXGI_FORMAT_R32_UINT, 0);
  app.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  Constants c{XMMatrixTranspose(ship_model * view * proj), XMMatrixTranspose(ship_model)};
  app.context->UpdateSubresource(app.constants.Get(), 0, nullptr, &c, 0, 0);
  app.context->DrawIndexed(app.ship_index_count, 0, 0);

  const float flame_intensity = ds5_demo_engine_flame_intensity(app.pose, app.last_frame.flight);
  std::vector<Vertex> flame_vertices = build_engine_flame_vertices(ship_head_pose(app), flame_intensity, app.flame_time);
  if (!flame_vertices.empty()) {
    ComPtr<ID3D11Buffer> flame_buffer = create_buffer(app.device.Get(), flame_vertices, D3D11_BIND_VERTEX_BUFFER);
    XMMATRIX flame_model = XMMatrixIdentity();
    draw_vertices(app, flame_buffer.Get(), static_cast<UINT>(flame_vertices.size()), D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
                  flame_model * view * proj, flame_model);
  }

  std::vector<Vertex> bullet_vertices = build_light_bullet_vertices(app.bullets);
  if (!bullet_vertices.empty()) {
    ComPtr<ID3D11Buffer> bullet_buffer = create_buffer(app.device.Get(), bullet_vertices, D3D11_BIND_VERTEX_BUFFER);
    XMMATRIX bullet_model = XMMatrixIdentity();
    draw_vertices(app, bullet_buffer.Get(), static_cast<UINT>(bullet_vertices.size()), D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
                  bullet_model * view * proj, bullet_model);
  }

  app.swap_chain->Present(1, 0);
  draw_debug_overlay(app);
}

void try_open_controller(DemoApp& app) {
  try {
    app.controller = DualSense::Controller::openFirstUsb(app.ds_context);
    app.has_controller = true;
  } catch (...) {
    app.has_controller = false;
  }
}

void try_find_speaker_endpoint(DemoApp& app) {
  uint32_t count = 0;
  ds5_result result = ds5_audio_enumerate_endpoints(app.ds_context.native(), nullptr, 0, &count);
  if (result != DS5_OK && result != DS5_E_INSUFFICIENT_BUFFER) return;
  std::vector<ds5_audio_endpoint> endpoints(count);
  for (auto& endpoint : endpoints) {
    endpoint.size = sizeof(endpoint);
    endpoint.version = DS5_STRUCT_VERSION;
  }
  if (count == 0 || ds5_audio_enumerate_endpoints(app.ds_context.native(), endpoints.data(), count, &count) != DS5_OK) return;
  for (const auto& endpoint : endpoints) {
    if (!endpoint.is_capture) {
      app.speaker_endpoint_id = endpoint.id;
      return;
    }
  }
}

void update_title(DemoApp& app) {
  wchar_t title[256];
  swprintf_s(title,
             L"DualSense Ship | %s%s | speed %.1f | target %d dist %.1f angle %.1f | auto %s | trigger %S | gyro %s | %S",
             app.has_controller ? L"USB DualSense" : (app.has_xinput ? L"XInput" : L"keyboard"),
             app.tactical_panel ? L" | tactical" : L"",
             app.pose.speed,
             app.debug.lockedTargetId,
             app.debug.lockedDistance,
             app.debug.lockedAngle,
             app.auto_follow ? L"on" : L"off",
             app.config.adaptiveTriggerEnabled ? ds5_demo_trigger_mode_name(app.trigger_mode) : "off",
             app.config.motionControlEnabled ? L"on" : L"off",
             app.last_event.c_str());
  SetWindowTextW(app.hwnd, title);
}

void initialize_targets(DemoApp& app) {
  app.targets.clear();
}

void initialize_feedback_zones(DemoApp& app) {
  app.feedback_zones = {
      {ShipFeedbackZoneKind::Pulse, -8.0f, 1.5f, 10.0f, 3.6f},
      {ShipFeedbackZoneKind::Heavy, 7.0f, 1.5f, 16.0f, 3.6f},
      {ShipFeedbackZoneKind::Rapid, -2.0f, 4.0f, 25.0f, 4.0f},
      {ShipFeedbackZoneKind::Sticky, 10.0f, -1.0f, 28.0f, 4.2f},
  };
}

void play_system_tone(DemoApp& app, float frequency, uint32_t duration_ms) {
  if (app.speaker_endpoint_id.empty()) return;
  ds5_audio_format format{};
  format.size = sizeof(format);
  format.version = DS5_STRUCT_VERSION;
  format.sample_rate = 48000;
  format.channels = 2;
  format.bits_per_sample = 16;
  const uint32_t frames = format.sample_rate * duration_ms / 1000u;
  std::vector<int16_t> pcm(frames * 2u);
  for (uint32_t i = 0; i < frames; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(format.sample_rate);
    const int16_t sample = static_cast<int16_t>(std::sin(t * frequency * 6.2831853f) * 6500.0f);
    pcm[i * 2u + 0u] = sample;
    pcm[i * 2u + 1u] = sample;
  }
  ds5_audio_play_pcm(app.ds_context.native(), app.speaker_endpoint_id.c_str(), pcm.data(),
                     static_cast<uint32_t>(pcm.size() * sizeof(int16_t)), &format);
}

bool load_wav_pcm16(const char* path, std::vector<uint8_t>& pcm, ds5_audio_format& format) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return false;
  std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  if (data.size() < 44 || std::memcmp(data.data(), "RIFF", 4) != 0 || std::memcmp(data.data() + 8, "WAVE", 4) != 0) return false;
  uint16_t audio_format = 0;
  uint16_t channels = 0;
  uint32_t sample_rate = 0;
  uint16_t bits = 0;
  size_t cursor = 12;
  size_t data_offset = 0;
  uint32_t data_size = 0;
  while (cursor + 8 <= data.size()) {
    const char* id = reinterpret_cast<const char*>(data.data() + cursor);
    uint32_t size = 0;
    std::memcpy(&size, data.data() + cursor + 4, 4);
    cursor += 8;
    if (cursor + size > data.size()) break;
    if (std::memcmp(id, "fmt ", 4) == 0 && size >= 16) {
      std::memcpy(&audio_format, data.data() + cursor, 2);
      std::memcpy(&channels, data.data() + cursor + 2, 2);
      std::memcpy(&sample_rate, data.data() + cursor + 4, 4);
      std::memcpy(&bits, data.data() + cursor + 14, 2);
    } else if (std::memcmp(id, "data", 4) == 0) {
      data_offset = cursor;
      data_size = size;
    }
    cursor += size + (size & 1u);
  }
  if (audio_format != 1 || channels == 0 || sample_rate == 0 || bits != 16 || data_offset == 0 || data_size == 0) return false;
  pcm.assign(data.begin() + data_offset, data.begin() + data_offset + data_size);
  format.size = sizeof(format);
  format.version = DS5_STRUCT_VERSION;
  format.sample_rate = sample_rate;
  format.channels = channels;
  format.bits_per_sample = bits;
  return true;
}

bool load_commander_ready_wav(std::vector<uint8_t>& pcm, ds5_audio_format& format) {
  const char* relative_paths[] = {
      "assets/commander_ready.wav",
      "../assets/commander_ready.wav",
      "../../assets/commander_ready.wav",
  };
  for (const char* path : relative_paths) {
    if (load_wav_pcm16(path, pcm, format)) return true;
  }
  const std::string exe_dir = executable_directory();
  if (!exe_dir.empty()) {
    const std::string from_exe = exe_dir + "\\..\\assets\\commander_ready.wav";
    if (load_wav_pcm16(from_exe.c_str(), pcm, format)) return true;
  }
  return false;
}

void play_commander_ready(DemoApp& app) {
  if (app.speaker_endpoint_id.empty()) return;
  std::vector<uint8_t> pcm;
  ds5_audio_format format{};
  if (load_commander_ready_wav(pcm, format)) {
    ds5_result result = ds5_audio_play_pcm(app.ds_context.native(), app.speaker_endpoint_id.c_str(), pcm.data(),
                                           static_cast<uint32_t>(pcm.size()), &format);
    if (result == DS5_OK) return;
  }
  play_system_tone(app, 880.0f, 80);
  play_system_tone(app, 660.0f, 100);
}

void log_event(DemoApp& app, const std::string& event) {
  app.last_event = event;
  OutputDebugStringA(("DualSense ship event: " + event + "\n").c_str());
  if (event == "target lock" || event == "lock failed") play_commander_ready(app);
  else if (event == "collision") play_system_tone(app, 180.0f, 120);
  else if (event.find("trigger mode") == 0) play_system_tone(app, 660.0f, 70);
  else if (event == "target lost") play_system_tone(app, 240.0f, 90);
}

ShipTarget* locked_target(DemoApp& app) {
  if (app.locked_target < 0 || app.locked_target >= static_cast<int>(app.targets.size())) return nullptr;
  ShipTarget& target = app.targets[app.locked_target];
  return target.alive ? &target : nullptr;
}

void update_target_lock(DemoApp& app, const SpaceShipInputFrame& frame) {
  if (app.targets.empty()) {
    app.locked_target = -1;
    app.auto_follow = false;
    app.tactical_panel = false;
    app.debug.lockedTargetId = -1;
    app.debug.lockedDistance = 0.0f;
    app.debug.lockedAngle = 0.0f;
    app.debug.autoFollow = false;
    app.debug.adaptiveTrigger = app.config.adaptiveTriggerEnabled && app.has_controller;
    app.debug.gyro = app.config.motionControlEnabled && app.has_controller;
    return;
  }

  if (frame.lockTarget) {
    try {
      if (app.has_controller) {
        app.controller.haptics().pattern(20, 55, 90);
        app.has_last_feedback = false;
      }
    } catch (...) {
      app.has_controller = false;
    }
    const int index = ds5_demo_find_lock_target(app.pose, app.targets, app.config);
    app.locked_target = index;
    log_event(app, index >= 0 ? "target lock" : "lock failed");
  }
  if (frame.toggleAutoFollow) {
    app.auto_follow = !app.auto_follow;
    app.tactical_panel = !app.tactical_panel;
    log_event(app, app.auto_follow ? "auto follow on" : "auto follow off");
  }
  ShipTarget* target = locked_target(app);
  if (target) {
    app.debug.lockedTargetId = target->id;
    app.debug.lockedDistance = ds5_demo_distance_to_target(app.pose, *target);
    app.debug.lockedAngle = ds5_demo_angle_to_target_degrees(app.pose, *target);
    if (app.debug.lockedDistance > app.config.autoFollowMaxDistance * 1.25f || app.debug.lockedAngle > 110.0f) {
      app.locked_target = -1;
      app.auto_follow = false;
      log_event(app, "target lost");
    }
  } else {
    app.debug.lockedTargetId = -1;
    app.debug.lockedDistance = 0.0f;
    app.debug.lockedAngle = 0.0f;
  }
  app.debug.autoFollow = app.auto_follow;
  app.debug.adaptiveTrigger = app.config.adaptiveTriggerEnabled && app.has_controller;
  app.debug.gyro = app.config.motionControlEnabled && app.has_controller;
}

int run(HINSTANCE instance) {
  DemoApp app;
  app.instance = instance;
  g_app = &app;
  create_window(app);
  create_device(app);
  create_assets(app);
  app.config = ds5_demo_load_config("config/ship_controls.cfg");
  initialize_targets(app);
  initialize_feedback_zones(app);
  try_open_controller(app);
  try_find_speaker_endpoint(app);
  DualSenseFeedbackManager feedback_manager;

  auto previous = std::chrono::steady_clock::now();
  MSG msg{};
  int title_counter = 0;
  while (app.running) {
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }

    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - previous).count();
    previous = now;
    dt = ds5_demo_clamp(dt, 0.0f, 0.05f);
    app.frame_dt = dt;

    SpaceShipInputFrame frame = merged_input_frame(app);
    app.last_frame = frame;
    if (app.was_firing && !frame.fire) {
      try {
        if (app.has_controller) {
          app.controller.haptics().rumble(0, 0);
          app.controller.triggers().off(false);
          app.has_last_feedback = false;
        }
      } catch (...) {
        app.has_controller = false;
      }
      log_event(app, "fire stop");
    }
    app.was_firing = frame.fire;
    if (frame.pause) {
      app.paused = !app.paused;
      log_event(app, app.paused ? "paused" : "resumed");
    }

    update_target_lock(app, frame);
    ds5_demo_update_camera(app.camera, frame, app.config, dt);
    ShipFeedbackZoneKind previous_zone = app.active_zone;
    app.active_zone = ds5_demo_find_feedback_zone(app.pose, app.feedback_zones);
    if (app.active_zone != previous_zone) {
      app.has_last_feedback = false;
      log_event(app, std::string("zone ") + ds5_demo_feedback_zone_name(app.active_zone));
    }

    ShipInput input = frame.flight;
    if (frame.boost) input.throttle = std::max(input.throttle, 1.0f);
    app.weapon_charging = app.trigger_mode == ShipTriggerMode::Charge && frame.fire && input.throttle > 0.6f;
    app.fire_cooldown = std::max(0.0f, app.fire_cooldown - dt);

    if (app.auto_follow) {
      input = ds5_demo_apply_auto_follow(app.pose, input, locked_target(app), app.config, app.debug);
    } else {
      app.debug.followStrength = 0.0f;
    }

    if (frame.dodge) {
      app.pose.x += std::sin(app.pose.yaw + 1.5708f) * (input.roll >= 0.0f ? 3.5f : -3.5f);
      log_event(app, "dodge");
      try {
        if (app.has_controller) app.controller.haptics().pattern(35, 80, 80);
      } catch (...) {}
    }
    if (frame.switchWeapon) {
      app.trigger_mode = ds5_demo_next_trigger_mode(app.trigger_mode);
      app.has_last_feedback = false;
      log_event(app, std::string("trigger mode ") + ds5_demo_trigger_mode_name(app.trigger_mode));
    }
    if (frame.fire && app.fire_cooldown <= 0.0f) {
      app.bullets.push_back(ds5_demo_spawn_light_bullet(ship_head_pose(app)));
      if (app.bullets.size() > 72u) {
        app.bullets.erase(app.bullets.begin(), app.bullets.begin() + static_cast<std::ptrdiff_t>(app.bullets.size() - 72u));
      }
      app.fire_cooldown = 0.11f;
      log_event(app, "light shot");
      try {
        if (app.has_controller) app.controller.haptics().pattern(12, 48, 28);
      } catch (...) {}
    }

    if (!app.paused) {
      ds5_demo_step_ship_tuned(app.pose, input, dt, app.config.turnSpeed, app.config.rollSpeed,
                               app.config.moveSpeed, app.config.brakeStrength, app.config.boostMultiplier,
                               app.camera.yaw, flight_pitch_from_camera(app.camera));
      app.flame_time += dt;
      ds5_demo_update_camera_follow(app.camera, app.pose, dt);
      ds5_demo_update_light_bullets(app.bullets, dt);
      for (const auto& target : app.targets) {
        if (target.alive && ds5_demo_distance_to_target(app.pose, target) < 1.8f) {
          app.health = ds5_demo_clamp(app.health - std::fabs(app.pose.speed) * 0.01f, 0.0f, 1.0f);
          app.pose.speed *= -0.35f;
          log_event(app, "collision");
          try {
            if (app.has_controller) app.controller.haptics().pattern(120, 120, 120);
          } catch (...) {}
        }
      }
    }

    feedback_manager.update(app, input, dt);
    render(app);

    if (++title_counter > 20) {
      update_title(app);
      title_counter = 0;
    }
  }
  reset_controller_feedback(app);
  return 0;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
  try {
    return run(instance);
  } catch (const std::exception& ex) {
    MessageBoxA(nullptr, ex.what(), "DualSense Ship Demo", MB_ICONERROR | MB_OK);
    return 1;
  }
}

int main() {
  return wWinMain(GetModuleHandleW(nullptr), nullptr, GetCommandLineW(), SW_SHOWDEFAULT);
}
