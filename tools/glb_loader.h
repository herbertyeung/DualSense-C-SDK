#ifndef DS5_GLB_LOADER_H
#define DS5_GLB_LOADER_H

#include <DirectXMath.h>

#include <cstdint>
#include <string>
#include <vector>

struct GlbVertex {
  DirectX::XMFLOAT3 position;
  DirectX::XMFLOAT3 normal;
  DirectX::XMFLOAT2 uv;
  DirectX::XMFLOAT3 color;
};

struct GlbMeshData {
  std::vector<GlbVertex> vertices;
  std::vector<uint32_t> indices;
  DirectX::XMFLOAT3 bounds_min{0, 0, 0};
  DirectX::XMFLOAT3 bounds_max{0, 0, 0};
};

bool ds5_demo_load_glb_mesh(const char* path, GlbMeshData* output, std::string* error);

#endif
