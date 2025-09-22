// simple_geometry_mesh.cpp
#include "sv/simple-mesh.hpp"

#include "sv/app.hpp"
#include "sv/common.hpp"
#include "sv/object_handle.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

namespace simple {

using namespace sv;

struct VertexPNV2
{
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
};

static auto
as_bytes(const std::vector<VertexPNV2>& v) -> std::span<const std::byte>
{
  return std::as_bytes(std::span{ v });
}

static auto
as_bytes(const std::vector<std::uint32_t>& v) -> std::span<const std::byte>
{
  return std::as_bytes(std::span{ v });
}

static auto
make_buffers(IContext& ctx,
             std::string_view name,
             const std::vector<VertexPNV2>& vertices,
             const std::vector<std::uint32_t>& indices)
  -> std::pair<Holder<BufferHandle>, Holder<BufferHandle>>
{
  BufferDescription vbd{ .data = as_bytes(vertices),
                         .usage = BufferUsageBits::Vertex |
                                  BufferUsageBits::Source |
                                  BufferUsageBits::Destination,
                         .storage = StorageType::HostVisible,
                         .size = vertices.size() * sizeof(VertexPNV2),
                         .debug_name = name };
  auto vb = VulkanDeviceBuffer::create(ctx, vbd);

  BufferDescription ibd{ .data = as_bytes(indices),
                         .usage = BufferUsageBits::Index |
                                  BufferUsageBits::Source |
                                  BufferUsageBits::Destination,
                         .storage = StorageType::HostVisible,
                         .size = indices.size() * sizeof(std::uint32_t),
                         .debug_name = name };
  auto ib = VulkanDeviceBuffer::create(ctx, ibd);

  return { std::move(vb), std::move(ib) };
}

static auto
compute_face_normal(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
  -> glm::vec3
{
  auto n = glm::cross(b - a, c - a);
  return glm::length2(n) > 0.f ? glm::normalize(n) : glm::vec3{ 0.f };
}

static auto
load_obj(std::string_view obj_path)
  -> std::pair<std::vector<VertexPNV2>, std::vector<std::uint32_t>>
{
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;

  tinyobj::ObjReaderConfig cfg{};
  cfg.triangulate = true;
  cfg.triangulation_method = "earcut";
  cfg.vertex_color = false;

  tinyobj::ObjReader reader{};

  if (!reader.ParseFromFile(std::string{ obj_path }, cfg))
    throw std::runtime_error("tinyobjloader: " + warn + err);

  attrib = reader.GetAttrib();
  shapes = reader.GetShapes();
  materials = reader.GetMaterials();
  warn = reader.Warning();
  err = reader.Error();

  std::vector<VertexPNV2> vertices;
  std::vector<std::uint32_t> indices;
  vertices.reserve(1024);
  indices.reserve(1024);

  for (const auto& s : shapes) {
    const auto& mesh = s.mesh;
    for (size_t f = 0; f < mesh.indices.size(); f += 3) {
      VertexPNV2 tri[3]{};
      for (int k = 0; k < 3; ++k) {
        const auto& idx = mesh.indices[f + k];

        if (idx.vertex_index < 0)
          throw std::runtime_error("OBJ has invalid vertex index");
        const int vi = 3 * idx.vertex_index;
        tri[k].position = {
          attrib.vertices[vi + 0],
          attrib.vertices[vi + 1],
          attrib.vertices[vi + 2],
        };

        if (idx.normal_index >= 0) {
          const int ni = 3 * idx.normal_index;
          tri[k].normal = {
            attrib.normals[ni + 0],
            attrib.normals[ni + 1],
            attrib.normals[ni + 2],
          };
        } else {
          tri[k].normal = { 0.f, 0.f, 0.f };
        }

        if (idx.texcoord_index >= 0) {
          const int ti = 2 * idx.texcoord_index;
          tri[k].uv = { attrib.texcoords[ti + 0], attrib.texcoords[ti + 1] };
        } else {
          tri[k].uv = { 0.f, 0.f };
        }
      }

      if (tri[0].normal == glm::vec3{ 0.f } &&
          tri[1].normal == glm::vec3{ 0.f } &&
          tri[2].normal == glm::vec3{ 0.f }) {
        const auto n = compute_face_normal(
          tri[0].position, tri[1].position, tri[2].position);
        tri[0].normal = tri[1].normal = tri[2].normal = n;
      }

      const auto base = static_cast<std::uint32_t>(vertices.size());
      vertices.push_back(tri[0]);
      vertices.push_back(tri[1]);
      vertices.push_back(tri[2]);
      indices.push_back(base + 0);
      indices.push_back(base + 1);
      indices.push_back(base + 2);
    }
  }

  return { std::move(vertices), std::move(indices) };
}

auto kind_to_path =
  [](const SimpleGeometryParams::Kind kind) -> std::string_view {
  switch (kind) {
    using Kind = SimpleGeometryParams::Kind;
    case Kind::Cube: {
      return "meshes/cube.obj";
    };
    case Kind::Capsule: {
      return "meshes/capsule.obj";
    }
  };

  return "meshes/cube.obj";
};

auto
SimpleGeometryMesh::create(IContext& ctx, const SimpleGeometryParams& p)
  -> SimpleGeometryMesh
{
  std::vector<VertexPNV2> vertices;
  std::vector<std::uint32_t> indices;

  auto obj_path = kind_to_path(p.kind);
  auto [v, i] = load_obj(
    obj_path); // expects SimpleGeometryParams{ obj_path, debug_name, ... }
  vertices = std::move(v);
  indices = std::move(i);

  auto [vb, ib] = make_buffers(ctx, p.debug_name, vertices, indices);

  SimpleGeometryMesh m{};
  m.vertex_buffer = std::move(vb);
  m.index_buffer = std::move(ib);
  m.index_count = static_cast<std::uint32_t>(indices.size());
  m.index_type = VK_INDEX_TYPE_UINT32;
  return m;
}

} // namespace simple
