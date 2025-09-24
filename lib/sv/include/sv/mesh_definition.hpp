#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "sv/common.hpp"
#include "sv/strong.hpp"

struct aiMesh;

namespace sv {

constexpr auto calculate_lods{ true };
constexpr auto max_lods{ 8ULL };
constexpr auto magic_header{ 0xFAB2C1U };

struct Material
{};

struct MeshHeader
{
  std::uint32_t magic{ magic_header };
  std::uint32_t mesh_serial_version{ 0x1001 };
  std::uint32_t mesh_count{ 0 };
  std::uint32_t index_data_size{ 0 };
  std::uint32_t vertex_data_size{ 0 };
};

struct Mesh
{
  std::uint32_t lod_count{ 1 };
  std::uint32_t index_offset{ 0 };
  std::uint32_t vertex_offset{ 0 };
  std::uint32_t vertex_count{ 0 };
  std::uint32_t material_index{ 0 };

  std::array<std::uint32_t, max_lods + 1> lod_offset{};

  auto get_lod_index_count(const std::uint32_t lod) const
  {
    return lod < lod_count ? lod_offset.at(lod + 1) - lod_offset.at(0) : 0;
  }
};

struct MeshData
{
  VertexInput streams{};

  std::vector<std::uint32_t> indices{};
  std::vector<std::uint8_t> vertices{};

  std::vector<Mesh> meshes{};
  std::vector<BoundingBox> aabbs{};
  std::vector<Material> materials;
  std::vector<std::string> texture_files{};
};

struct MeshFile
{
  MeshHeader header{};
  MeshData mesh{};
};

auto
load_mesh_data(const std::string_view) -> std::optional<MeshFile>;
auto
convert_assimp_mesh(const aiMesh*, MeshData&, VertexOffset&, IndexOffset&)
  -> Mesh;
auto
load_mesh_file(const std::string_view) -> std::optional<MeshData>;
auto
save_mesh_data(const std::string_view, const MeshData&) -> bool;

}
