#include "glb_loader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Json {
  enum Type { Null, Bool, Number, String, Array, Object } type = Null;
  double number = 0.0;
  bool boolean = false;
  std::string string;
  std::vector<Json> array;
  std::map<std::string, Json> object;

  const Json& operator[](const char* key) const {
    static Json empty;
    auto it = object.find(key);
    return it == object.end() ? empty : it->second;
  }
  const Json& operator[](size_t index) const {
    static Json empty;
    return index < array.size() ? array[index] : empty;
  }
  bool has(const char* key) const { return object.find(key) != object.end(); }
  int as_int(int fallback = 0) const { return type == Number ? static_cast<int>(number) : fallback; }
  float as_float(float fallback = 0.0f) const { return type == Number ? static_cast<float>(number) : fallback; }
};

struct Parser {
  const std::string& text;
  size_t pos = 0;
  void ws() { while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos; }
  bool consume(char c) { ws(); if (pos < text.size() && text[pos] == c) { ++pos; return true; } return false; }
  Json parse() { ws(); return parse_value(); }
  Json parse_value() {
    ws();
    if (pos >= text.size()) return {};
    if (text[pos] == '{') return parse_object();
    if (text[pos] == '[') return parse_array();
    if (text[pos] == '"') return parse_string();
    if (text.compare(pos, 4, "true") == 0) { pos += 4; Json j; j.type = Json::Bool; j.boolean = true; return j; }
    if (text.compare(pos, 5, "false") == 0) { pos += 5; Json j; j.type = Json::Bool; return j; }
    if (text.compare(pos, 4, "null") == 0) { pos += 4; return {}; }
    return parse_number();
  }
  Json parse_object() {
    Json j; j.type = Json::Object; consume('{');
    while (!consume('}')) {
      Json key = parse_string();
      consume(':');
      j.object[key.string] = parse_value();
      consume(',');
    }
    return j;
  }
  Json parse_array() {
    Json j; j.type = Json::Array; consume('[');
    while (!consume(']')) {
      j.array.push_back(parse_value());
      consume(',');
    }
    return j;
  }
  Json parse_string() {
    Json j; j.type = Json::String; consume('"');
    while (pos < text.size()) {
      char c = text[pos++];
      if (c == '"') break;
      if (c == '\\' && pos < text.size()) {
        char e = text[pos++];
        if (e == '"' || e == '\\' || e == '/') j.string.push_back(e);
        else if (e == 'n') j.string.push_back('\n');
        else if (e == 'r') j.string.push_back('\r');
        else if (e == 't') j.string.push_back('\t');
        else if (e == 'b') j.string.push_back('\b');
        else if (e == 'f') j.string.push_back('\f');
        else if (e == 'u') { pos = std::min(pos + 4, text.size()); j.string.push_back('?'); }
      } else {
        j.string.push_back(c);
      }
    }
    return j;
  }
  Json parse_number() {
    Json j; j.type = Json::Number;
    size_t start = pos;
    while (pos < text.size() && (std::isdigit(static_cast<unsigned char>(text[pos])) || text[pos] == '-' || text[pos] == '+' || text[pos] == '.' || text[pos] == 'e' || text[pos] == 'E')) ++pos;
    j.number = std::strtod(text.c_str() + start, nullptr);
    return j;
  }
};

uint32_t read_u32(const std::vector<uint8_t>& data, size_t offset) {
  uint32_t value = 0;
  std::memcpy(&value, data.data() + offset, sizeof(value));
  return value;
}

std::vector<uint8_t> read_file(const char* path) {
  std::ifstream file(path, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

DirectX::XMFLOAT3 material_color(const Json& gltf, int material_index) {
  static const DirectX::XMFLOAT3 palette[] = {
      {0.82f, 0.78f, 0.68f}, {0.05f, 0.06f, 0.07f}, {0.35f, 0.36f, 0.36f},
      {0.55f, 0.55f, 0.52f}, {0.18f, 0.22f, 0.25f}, {1.0f, 0.35f, 0.08f},
      {0.12f, 0.55f, 1.0f}, {0.9f, 0.05f, 0.04f}, {0.02f, 0.025f, 0.03f}};
  if (material_index >= 0 && material_index < static_cast<int>(std::size(palette))) return palette[material_index];
  const Json& mat = gltf["materials"][static_cast<size_t>(std::max(material_index, 0))];
  const Json& factor = mat["pbrMetallicRoughness"]["baseColorFactor"];
  if (factor.type == Json::Array && factor.array.size() >= 3) {
    return {factor[static_cast<size_t>(0)].as_float(1), factor[static_cast<size_t>(1)].as_float(1), factor[static_cast<size_t>(2)].as_float(1)};
  }
  return {0.8f, 0.8f, 0.85f};
}

struct AccessorView {
  const uint8_t* data = nullptr;
  int count = 0;
  int component_type = 0;
  int stride = 0;
  std::string type;
};

int component_size(int component_type) {
  switch (component_type) {
    case 5120: case 5121: return 1;
    case 5122: case 5123: return 2;
    case 5125: case 5126: return 4;
    default: return 0;
  }
}

int type_components(const std::string& type) {
  if (type == "SCALAR") return 1;
  if (type == "VEC2") return 2;
  if (type == "VEC3") return 3;
  if (type == "VEC4") return 4;
  return 0;
}

AccessorView accessor_view(const Json& gltf, const std::vector<uint8_t>& bin, int accessor_index) {
  AccessorView view{};
  if (accessor_index < 0) return view;
  const Json& accessor = gltf["accessors"][static_cast<size_t>(accessor_index)];
  const Json& buffer_view = gltf["bufferViews"][static_cast<size_t>(accessor["bufferView"].as_int())];
  const int accessor_offset = accessor["byteOffset"].as_int(0);
  const int view_offset = buffer_view["byteOffset"].as_int(0);
  view.data = bin.data() + view_offset + accessor_offset;
  view.count = accessor["count"].as_int();
  view.component_type = accessor["componentType"].as_int();
  view.type = accessor["type"].string;
  view.stride = buffer_view["byteStride"].as_int(component_size(view.component_type) * type_components(view.type));
  return view;
}

float read_float_component(const AccessorView& view, int index, int component) {
  const uint8_t* ptr = view.data + index * view.stride + component * component_size(view.component_type);
  if (view.component_type == 5126) {
    float value = 0.0f;
    std::memcpy(&value, ptr, sizeof(value));
    return value;
  }
  return 0.0f;
}

uint32_t read_index_component(const AccessorView& view, int index) {
  const uint8_t* ptr = view.data + index * view.stride;
  if (view.component_type == 5125) { uint32_t v = 0; std::memcpy(&v, ptr, 4); return v; }
  if (view.component_type == 5123) { uint16_t v = 0; std::memcpy(&v, ptr, 2); return v; }
  if (view.component_type == 5121) return *ptr;
  return 0;
}

void update_bounds(GlbMeshData* mesh, const DirectX::XMFLOAT3& p, bool first) {
  if (first) {
    mesh->bounds_min = mesh->bounds_max = p;
    return;
  }
  mesh->bounds_min.x = std::min(mesh->bounds_min.x, p.x);
  mesh->bounds_min.y = std::min(mesh->bounds_min.y, p.y);
  mesh->bounds_min.z = std::min(mesh->bounds_min.z, p.z);
  mesh->bounds_max.x = std::max(mesh->bounds_max.x, p.x);
  mesh->bounds_max.y = std::max(mesh->bounds_max.y, p.y);
  mesh->bounds_max.z = std::max(mesh->bounds_max.z, p.z);
}

DirectX::XMMATRIX node_matrix(const Json& node) {
  const Json& matrix = node["matrix"];
  if (matrix.type == Json::Array && matrix.array.size() == 16) {
    float m[16]{};
    for (size_t i = 0; i < 16; ++i) m[i] = matrix[i].as_float();
    return DirectX::XMMatrixSet(
        m[0], m[4], m[8], m[12],
        m[1], m[5], m[9], m[13],
        m[2], m[6], m[10], m[14],
        m[3], m[7], m[11], m[15]);
  }

  DirectX::XMFLOAT3 translation{0, 0, 0};
  DirectX::XMFLOAT4 rotation{0, 0, 0, 1};
  DirectX::XMFLOAT3 scale{1, 1, 1};
  const Json& t = node["translation"];
  const Json& r = node["rotation"];
  const Json& s = node["scale"];
  if (t.type == Json::Array && t.array.size() >= 3) translation = {t[static_cast<size_t>(0)].as_float(), t[static_cast<size_t>(1)].as_float(), t[static_cast<size_t>(2)].as_float()};
  if (r.type == Json::Array && r.array.size() >= 4) rotation = {r[static_cast<size_t>(0)].as_float(), r[static_cast<size_t>(1)].as_float(), r[static_cast<size_t>(2)].as_float(), r[static_cast<size_t>(3)].as_float()};
  if (s.type == Json::Array && s.array.size() >= 3) scale = {s[static_cast<size_t>(0)].as_float(1), s[static_cast<size_t>(1)].as_float(1), s[static_cast<size_t>(2)].as_float(1)};

  return DirectX::XMMatrixScaling(scale.x, scale.y, scale.z) *
         DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotation)) *
         DirectX::XMMatrixTranslation(translation.x, translation.y, translation.z);
}

void append_mesh_primitive(const Json& gltf, const std::vector<uint8_t>& bin, const Json& prim,
                           const DirectX::XMMATRIX& world, GlbMeshData* output, bool* have_bounds) {
  if (prim["mode"].as_int(4) != 4) return;
  const Json& attrs = prim["attributes"];
  if (!attrs.has("POSITION")) return;

  AccessorView pos = accessor_view(gltf, bin, attrs["POSITION"].as_int());
  AccessorView normal = accessor_view(gltf, bin, attrs["NORMAL"].as_int(-1));
  AccessorView uv = accessor_view(gltf, bin, attrs["TEXCOORD_0"].as_int(-1));
  AccessorView idx = accessor_view(gltf, bin, prim["indices"].as_int(-1));
  const DirectX::XMFLOAT3 color = material_color(gltf, prim["material"].as_int(-1));
  const uint32_t base = static_cast<uint32_t>(output->vertices.size());

  for (int i = 0; i < pos.count; ++i) {
    DirectX::XMFLOAT3 local_position{read_float_component(pos, i, 0), read_float_component(pos, i, 1), read_float_component(pos, i, 2)};
    DirectX::XMFLOAT3 local_normal = normal.data
        ? DirectX::XMFLOAT3{read_float_component(normal, i, 0), read_float_component(normal, i, 1), read_float_component(normal, i, 2)}
        : DirectX::XMFLOAT3{0, 1, 0};

    DirectX::XMVECTOR wp = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&local_position), world);
    DirectX::XMVECTOR wn = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&local_normal), world));

    GlbVertex v{};
    DirectX::XMStoreFloat3(&v.position, wp);
    DirectX::XMStoreFloat3(&v.normal, wn);
    v.uv = uv.data ? DirectX::XMFLOAT2{read_float_component(uv, i, 0), read_float_component(uv, i, 1)} : DirectX::XMFLOAT2{0, 0};
    v.color = color;
    update_bounds(output, v.position, !*have_bounds);
    *have_bounds = true;
    output->vertices.push_back(v);
  }

  if (idx.data) {
    for (int i = 0; i < idx.count; ++i) output->indices.push_back(base + read_index_component(idx, i));
  } else {
    for (int i = 0; i < pos.count; ++i) output->indices.push_back(base + static_cast<uint32_t>(i));
  }
}

void append_mesh(const Json& gltf, const std::vector<uint8_t>& bin, int mesh_index,
                 const DirectX::XMMATRIX& world, GlbMeshData* output, bool* have_bounds) {
  if (mesh_index < 0 || mesh_index >= static_cast<int>(gltf["meshes"].array.size())) return;
  const Json& mesh = gltf["meshes"][static_cast<size_t>(mesh_index)];
  for (const Json& prim : mesh["primitives"].array) {
    append_mesh_primitive(gltf, bin, prim, world, output, have_bounds);
  }
}

void append_node_recursive(const Json& gltf, const std::vector<uint8_t>& bin, int node_index,
                           const DirectX::XMMATRIX& parent, GlbMeshData* output, bool* have_bounds) {
  if (node_index < 0 || node_index >= static_cast<int>(gltf["nodes"].array.size())) return;
  const Json& node = gltf["nodes"][static_cast<size_t>(node_index)];
  const DirectX::XMMATRIX world = node_matrix(node) * parent;
  if (node.has("mesh")) append_mesh(gltf, bin, node["mesh"].as_int(-1), world, output, have_bounds);
  const Json& children = node["children"];
  if (children.type == Json::Array) {
    for (const Json& child : children.array) {
      append_node_recursive(gltf, bin, child.as_int(-1), world, output, have_bounds);
    }
  }
}

}  // namespace

bool ds5_demo_load_glb_mesh(const char* path, GlbMeshData* output, std::string* error) {
  if (!output) return false;
  output->vertices.clear();
  output->indices.clear();
  std::vector<uint8_t> data = read_file(path);
  if (data.size() < 20 || std::memcmp(data.data(), "glTF", 4) != 0) {
    if (error) *error = "not a GLB file";
    return false;
  }

  size_t offset = 12;
  std::string json_text;
  std::vector<uint8_t> bin;
  while (offset + 8 <= data.size()) {
    uint32_t length = read_u32(data, offset);
    uint32_t type = read_u32(data, offset + 4);
    offset += 8;
    if (offset + length > data.size()) break;
    if (type == 0x4E4F534A) json_text.assign(reinterpret_cast<const char*>(data.data() + offset), length);
    if (type == 0x004E4942) bin.assign(data.begin() + offset, data.begin() + offset + length);
    offset += length;
  }
  if (json_text.empty() || bin.empty()) {
    if (error) *error = "missing GLB JSON or BIN chunk";
    return false;
  }

  Json gltf = Parser{json_text}.parse();
  bool have_bounds = false;
  const Json& scene = gltf["scenes"][static_cast<size_t>(gltf["scene"].as_int(0))];
  const Json& scene_nodes = scene["nodes"];
  if (scene_nodes.type == Json::Array) {
    for (const Json& node : scene_nodes.array) {
      append_node_recursive(gltf, bin, node.as_int(-1), DirectX::XMMatrixIdentity(), output, &have_bounds);
    }
  }

  if (output->vertices.empty()) {
    for (size_t i = 0; i < gltf["meshes"].array.size(); ++i) {
      append_mesh(gltf, bin, static_cast<int>(i), DirectX::XMMatrixIdentity(), output, &have_bounds);
    }
  }

  if (output->vertices.empty() || output->indices.empty()) {
    if (error) *error = "no supported triangle mesh primitives";
    return false;
  }
  return true;
}
