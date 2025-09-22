#pragma once

#include "sv/abstract_command_buffer.hpp"
#include "sv/common.hpp"
#include "sv/object_handle.hpp"
#include "sv/object_holder.hpp"

#include <array>
#include <glm/glm.hpp>
#include <vector>

namespace sv {

class LineCanvas3D
{
  glm::mat4 mvp{ 1.0F };
  struct LineData
  {
    glm::vec4 position;
    glm::vec4 colour;
  };

  std::vector<LineData> lines{};
  Holder<ShaderModuleHandle> line_shader;
  Holder<GraphicsPipelineHandle> line_pipeline;

  static constexpr auto max_drawables = 3U;
  std::array<Holder<BufferHandle>, max_drawables> lines_buffer{};
  std::uint32_t lines_samples = 1;
  std::array<std::uint32_t, max_drawables> current_buffer_sizes{};
  std::uint32_t current_frame{ 0 };

public:
  auto set_mvp(const glm::mat4& new_mvp) { mvp = new_mvp; }
  auto clear() { lines.clear(); }
  auto line(const glm::vec3& p1, const glm::vec3& p2, const glm::vec4& c)
    -> void;
  auto plane(const glm::vec3& orig,
             const glm::vec3& v1,
             const glm::vec3& v2,
             int n1,
             int n2,
             float s1,
             float s2,
             const glm::vec4& colour,
             const glm::vec4& outline_colour) -> void;
  auto box(const glm::mat4& m, const BoundingBox& box, const glm::vec4& colour)
    -> void;
  auto box(const glm::mat4& m, const glm::vec3& size, const glm::vec4& colour)
    -> void;
  auto frustum(const glm::mat4& view,
               const glm::mat4& proj,
               const glm::vec4& colour) -> void;
  auto render(IContext&,
              const Framebuffer&,
              ICommandBuffer&,
              std::uint32_t num_samples = 1) -> void;
};

}