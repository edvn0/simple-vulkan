#pragma once

#include "sv/common.hpp"
#include "sv/buffer.hpp"
#include "sv/abstract_context.hpp"

#include <string_view>

namespace simple {

struct SimpleGeometryParams
{
  enum class Kind
  {
    Cube,
    Capsule
  };
  Kind kind{ Kind::Cube };
  glm::vec3 half_extents{ 0.5f, 0.5f, 0.5f };
  float radius{ 0.5f };
  float half_length{ 0.5f };
  std::uint32_t slices{ 32 };
  std::uint32_t stacks{ 16 };
  std::string_view debug_name{ "SimpleGeometry" };
};

struct SimpleGeometryMesh
{
  sv::Holder<sv::BufferHandle> vertex_buffer;
  sv::Holder<sv::BufferHandle> index_buffer;
  std::uint32_t index_count{ 0 };
  VkIndexType index_type{ VK_INDEX_TYPE_UINT32 };

  static auto create(sv::IContext& ctx, const SimpleGeometryParams& p)
    -> SimpleGeometryMesh;
};

}