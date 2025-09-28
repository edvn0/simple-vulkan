#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "sv/common.hpp"
#include "sv/material_definition.hpp"
#include "sv/object_handle.hpp"
#include "sv/strong.hpp"

struct aiMesh;

namespace sv {

constexpr auto calculate_lods{ true };
constexpr auto max_lods{ 8ULL };
constexpr auto magic_header{ 0xFAB2C1U };
constexpr auto serial_version{ 0x1001 }; // was 0x1001

struct MeshHeader
{
  std::uint32_t magic{ magic_header };
  std::uint32_t mesh_serial_version{ serial_version };
  std::uint32_t mesh_count{ 0 };
  std::uint32_t index_data_size{ 0 };
  std::uint32_t vertex_data_size{ 0 };
  std::uint32_t material_count{ 0 };
  std::uint32_t texture_count{ 0 };
  std::uint32_t texture_data_size{ 0 };
};

struct Mesh
{
  std::uint32_t lod_count{ 1 };
  std::uint32_t index_offset{ 0 };
  std::uint32_t vertex_offset{ 0 };
  std::uint32_t vertex_count{ 0 };
  std::uint32_t material_index{ 0 };

  std::array<std::uint32_t, max_lods + 1> lod_offset{};

  [[nodiscard]] auto get_lod_index_count(const std::uint32_t lod) const
  {
    return lod < lod_count ? lod_offset.at(static_cast<std::uint64_t>(lod) + 1) - lod_offset.at(0) : 0;
  }
};

enum class MaterialSlot : std::uint8_t
{
  emissive,
  base_color,
  normal,
  metallic,
  roughness,
  opacity
};

struct PendingTextureReference
{
  std::size_t material_idx;
  MaterialSlot slot;
  std::string key; // "*N"
};

struct CompressedTexture
{
  std::vector<std::byte> bytes;
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t mip_levels{};
  std::uint32_t format{};
};

struct MeshData
{
  VertexInput streams{};

  std::vector<std::uint32_t> indices{};
  std::vector<std::uint8_t> vertices{};

  std::vector<Mesh> meshes{};
  std::vector<BoundingBox> aabbs{};
  std::vector<Material> materials{};
  std::vector<CompressedTexture> compressed_textures{};
};

struct MeshFile
{
  MeshHeader header{};
  MeshData mesh{};
};

class RenderMesh
{

  MeshFile file{};
  Holder<BufferHandle> vertex_buffer;
  Holder<BufferHandle> index_buffer;
  Holder<BufferHandle> indirect_buffer;
  struct DrawData
  {
    std::uint32_t transform_index{ 0 };
    std::uint32_t material_index{ 0 };
  };
  Holder<BufferHandle> draw_data_buffer;
  struct Transform
  {
    glm::mat4 transform;
  };
  Holder<BufferHandle> transform_buffer;
  Holder<BufferHandle> material_buffer;

public:
  static auto create(IContext&, std::string_view)
    -> std::optional<RenderMesh>;

  [[nodiscard]] auto get_file() const -> const auto& { return file; }
  [[nodiscard]] auto get_vertex_buffer() const -> const auto& { return vertex_buffer; }
  [[nodiscard]] auto get_index_buffer() const -> const auto& { return index_buffer; }
};

auto
load_mesh_data_materials(std::string_view, MeshData&);
auto
save_mesh_data_materials(std::string_view, const MeshData&) -> void;
auto
load_mesh_file(std::string_view) -> std::optional<MeshFile>;
auto
save_mesh_file(std::string_view, const MeshFile&) -> void;

auto
load_mesh_data(std::string_view) -> std::optional<MeshData>;
auto
save_mesh_data(std::string_view, const MeshData&) -> bool;

}
