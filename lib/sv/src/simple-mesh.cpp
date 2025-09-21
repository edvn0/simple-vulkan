// simple_geometry_mesh.cpp
#include "sv/simple-mesh.hpp"

#include "sv/app.hpp"
#include "sv/common.hpp"
#include "sv/object_handle.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <cmath>
#include <glm/gtc/constants.hpp>
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
generate_cube(const glm::vec3 he)
  -> std::pair<std::vector<VertexPNV2>, std::vector<std::uint32_t>>
{
  const glm::vec3 p[8] = {
    { -he.x, -he.y, -he.z }, { he.x, -he.y, -he.z }, { he.x, he.y, -he.z },
    { -he.x, he.y, -he.z },  { -he.x, -he.y, he.z }, { he.x, -he.y, he.z },
    { he.x, he.y, he.z },    { -he.x, he.y, he.z },
  };

  struct F
  {
    int i0, i1, i2, i3;
    glm::vec3 n;
  } faces[6] = {
    { 0, 1, 2, 3, { 0, 0, -1 } }, { 4, 5, 6, 7, { 0, 0, 1 } },
    { 0, 4, 5, 1, { 0, -1, 0 } }, { 3, 2, 6, 7, { 0, 1, 0 } },
    { 1, 5, 6, 2, { 1, 0, 0 } },  { 0, 3, 7, 4, { -1, 0, 0 } },
  };

  std::vector<VertexPNV2> v;
  v.reserve(24);
  std::vector<std::uint32_t> i;
  i.reserve(36);

  for (int f = 0; f < 6; ++f) {
    const glm::vec2 uvs[4] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
    const auto base = static_cast<std::uint32_t>(v.size());
    const int vids[4] = { faces[f].i0, faces[f].i1, faces[f].i2, faces[f].i3 };
    for (int k = 0; k < 4; ++k)
      v.push_back(VertexPNV2{ p[vids[k]], faces[f].n, uvs[k] });
    i.insert(i.end(),
             { base + 0, base + 1, base + 2, base + 0, base + 2, base + 3 });
  }
  return { std::move(v), std::move(i) };
}

static auto
generate_capsule(float r,
                 float half_len,
                 std::uint32_t slices,
                 std::uint32_t stacks)
  -> std::pair<std::vector<VertexPNV2>, std::vector<std::uint32_t>>
{
  slices = std::max<std::uint32_t>(3, slices);
  stacks = std::max<std::uint32_t>(2, stacks);
  const std::uint32_t cyl_segments = std::max<std::uint32_t>(1, stacks);

  struct Ring
  {
    float y;
    float ring_r;
    enum class Zone
    {
      Bottom,
      Cyl,
      Top
    } zone;
  };
  std::vector<Ring> rings;
  rings.reserve(2 * stacks + cyl_segments + 1);

  for (std::uint32_t i = 0; i <= stacks; ++i) {
    float t = float(i) / float(stacks);
    float theta = -glm::half_pi<float>() + t * glm::half_pi<float>();
    rings.push_back({ -half_len + r * std::sin(theta),
                      r * std::cos(theta),
                      Ring::Zone::Bottom });
  }
  for (std::uint32_t j = 1; j < cyl_segments; ++j) {
    float t = float(j) / float(cyl_segments);
    rings.push_back({ -half_len + 2.f * half_len * t, r, Ring::Zone::Cyl });
  }
  for (std::uint32_t i = 1; i <= stacks; ++i) {
    float t = float(i) / float(stacks);
    float theta = t * glm::half_pi<float>();
    rings.push_back(
      { half_len + r * std::sin(theta), r * std::cos(theta), Ring::Zone::Top });
  }

  const std::uint32_t ring_count = static_cast<std::uint32_t>(rings.size());
  std::vector<VertexPNV2> v;
  v.reserve(ring_count * slices);
  std::vector<std::uint32_t> idx;
  idx.reserve((ring_count - 1) * slices * 6);

  for (std::uint32_t ri = 0; ri < ring_count; ++ri) {
    const auto& ring = rings[ri];
    for (std::uint32_t s = 0; s < slices; ++s) {
      float u = float(s) / float(slices);
      float phi = u * glm::two_pi<float>();
      float cx = std::cos(phi), sz = std::sin(phi);
      glm::vec3 pos{ ring.ring_r * cx, ring.y, ring.ring_r * sz };
      glm::vec3 n;
      if (ring.zone == Ring::Zone::Cyl)
        n = glm::normalize(glm::vec3{ cx, 0.f, sz });
      else {
        float cy = (ring.zone == Ring::Zone::Bottom) ? -half_len : +half_len;
        n = glm::normalize(glm::vec3{ pos.x, pos.y - cy, pos.z });
      }
      float vcoord = float(ri) / float(ring_count - 1);
      v.push_back(VertexPNV2{ pos, n, glm::vec2{ u, vcoord } });
    }
  }

  auto vert_index = [slices](std::uint32_t r, std::uint32_t s) {
    return r * slices + (s % slices);
  };
  for (std::uint32_t current_ring = 0; current_ring < ring_count - 1; ++current_ring) {
    for (std::uint32_t s = 0; s < slices; ++s) {
      auto a = vert_index(current_ring, s);
      auto b = vert_index(current_ring, s + 1);
      auto c = vert_index(current_ring + 1, s + 1);
      auto d = vert_index(current_ring + 1, s);
      idx.insert(idx.end(), { a, b, c, a, c, d });
    }
  }

  return { std::move(v), std::move(idx) };
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

auto
SimpleGeometryMesh::create(IContext& ctx, const SimpleGeometryParams& p)
  -> SimpleGeometryMesh
{
  std::vector<VertexPNV2> vertices;
  std::vector<std::uint32_t> indices;

  if (p.kind == SimpleGeometryParams::Kind::Cube) {
    std::tie(vertices, indices) = generate_cube(p.half_extents);
  } else {
    std::tie(vertices, indices) =
      generate_capsule(p.radius, p.half_length, p.slices, p.stacks);
  }

  auto [vb, ib] = make_buffers(ctx, p.debug_name, vertices, indices);

  SimpleGeometryMesh m{};
  m.vertex_buffer = std::move(vb);
  m.index_buffer = std::move(ib);
  m.index_count = static_cast<std::uint32_t>(indices.size());
  m.index_type = VK_INDEX_TYPE_UINT32;
  return m;
}

} // namespace sv
