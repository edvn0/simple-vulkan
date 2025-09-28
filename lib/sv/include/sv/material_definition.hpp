#pragma once

#include "sv/common.hpp"

#include <cstdint>
#include <glm/glm.hpp>

namespace sv {

enum class MaterialFlagBits : std::uint32_t
{
  CastsShadow = bit(0),
  ReceiveShadow = bit(1),
  Transparent = bit(2),
};
BIT_FIELD(MaterialFlagBits)

struct Material
{
  glm::vec4 emissive_factor{ 0.0F };
  glm::vec4 base_colour_factor{ 1.0F };
  float roughness{ 1.0F };
  float transparency_factor{ 1.0F };
  float alpha_test{ 0.0F };
  float metallic_factor{ 0.0F };

  std::int32_t base_colour_texture{ -1 };
  std::int32_t emissive_texture{ -1 };
  std::int32_t normal_texture{ -1 };
  std::int32_t opacity_texture{ -1 };
  std::int32_t metallic_texture{ -1 };
  std::int32_t roughness_texture{ -1 };

  MaterialFlagBits material_flags{
    MaterialFlagBits::CastsShadow | MaterialFlagBits::ReceiveShadow,
  };
};

}