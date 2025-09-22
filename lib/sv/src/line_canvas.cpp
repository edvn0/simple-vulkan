#include "sv/line_canvas.hpp"

#include <algorithm>
#include <cstring>
#include <glm/ext/matrix_transform.hpp>

#include "sv/buffer.hpp"
#include "sv/common.hpp"
#include "sv/context.hpp"
#include "sv/pipeline.hpp"

namespace sv {

auto
LineCanvas3D::line(const glm::vec3& p1, const glm::vec3& p2, const glm::vec4& c)
  -> void
{
  lines.emplace_back(glm::vec4{ p1, 1.0F }, c);
  lines.emplace_back(glm::vec4{ p2, 1.0F }, c);
}

auto
LineCanvas3D::plane(const glm::vec3& o,
                    const glm::vec3& v1,
                    const glm::vec3& v2,
                    int n1,
                    int n2,
                    float s1,
                    float s2,
                    const glm::vec4& colour,
                    const glm::vec4& outline_colour) -> void
{
  line(o - s1 / 2.0f * v1 - s2 / 2.0f * v2,
       o - s1 / 2.0f * v1 + s2 / 2.0f * v2,
       outline_colour);
  line(o + s1 / 2.0f * v1 - s2 / 2.0f * v2,
       o + s1 / 2.0f * v1 + s2 / 2.0f * v2,
       outline_colour);
  line(o - s1 / 2.0f * v1 + s2 / 2.0f * v2,
       o + s1 / 2.0f * v1 + s2 / 2.0f * v2,
       outline_colour);
  line(o - s1 / 2.0f * v1 - s2 / 2.0f * v2,
       o + s1 / 2.0f * v1 - s2 / 2.0f * v2,
       outline_colour);
  for (auto i = 1; i < n1; i++) {
    const float t = ((float)i - (float)n1 / 2.0f) * s1 / (float)n1;
    const auto o1 = o + t * v1;
    line(o1 - s2 / 2.0f * v2, o1 + s2 / 2.0f * v2, colour);
  }
  for (auto i = 1; i < n2; i++) {
    const float t = ((float)i - (float)n2 / 2.0f) * s2 / (float)n2;
    const auto o2 = o + t * v2;
    line(o2 - s1 / 2.0f * v1, o2 + s1 / 2.0f * v1, colour);
  }
}

auto
LineCanvas3D::box(const glm::mat4& m,
                  const glm::vec3& size,
                  const glm::vec4& color) -> void
{
  std::array points = {
    glm::vec3(+size.x, +size.y, +size.z), glm::vec3(+size.x, +size.y, -size.z),
    glm::vec3(+size.x, -size.y, +size.z), glm::vec3(+size.x, -size.y, -size.z),
    glm::vec3(-size.x, +size.y, +size.z), glm::vec3(-size.x, +size.y, -size.z),
    glm::vec3(-size.x, -size.y, +size.z), glm::vec3(-size.x, -size.y, -size.z)
  };
  for (auto& p : points)
    p = glm::vec3(m * glm::vec4(p, 1.f));

  line(points.at(0), points.at(1), color);
  line(points.at(2), points.at(3), color);
  line(points.at(4), points.at(5), color);
  line(points.at(6), points.at(7), color);
  line(points.at(0), points.at(2), color);
  line(points.at(1), points.at(3), color);
  line(points.at(4), points.at(6), color);
  line(points.at(5), points.at(7), color);
  line(points.at(0), points.at(4), color);
  line(points.at(1), points.at(5), color);
  line(points.at(2), points.at(6), color);
  line(points.at(3), points.at(7), color);
}

auto
LineCanvas3D::box(const glm::mat4& m,
                  const BoundingBox& bounding_box,
                  const glm::vec4& color) -> void
{
  box(m * glm::translate(glm::mat4(1.f),
                         0.5f * (bounding_box.min() + bounding_box.max())),
      0.5f * glm::vec3(bounding_box.max() - bounding_box.min()),
      color);
}

void
LineCanvas3D::frustum(const glm::mat4& view_matrix,
                      const glm::mat4& projection_matrix,
                      const glm::vec4& color)
{
  constexpr std::array corners = {
    glm::vec3(-1, -1, -1), glm::vec3(+1, -1, -1), glm::vec3(+1, +1, -1),
    glm::vec3(-1, +1, -1), glm::vec3(-1, -1, +1), glm::vec3(+1, -1, +1),
    glm::vec3(+1, +1, +1), glm::vec3(-1, +1, +1)
  };
  std::array<glm::vec3, 8> pp{};
  for (auto i = 0U; i < 8; i++) {
    glm::vec4 q = glm::inverse(view_matrix) * glm::inverse(projection_matrix) *
                  glm::vec4(corners.at(i), 1.0f);
    pp[i] = glm::vec3(q.x / q.w, q.y / q.w, q.z / q.w);
  }
  line(pp[0], pp[4], color);
  line(pp[1], pp[5], color);
  line(pp[2], pp[6], color);
  line(pp[3], pp[7], color);
  line(pp[0], pp[1], color);
  line(pp[1], pp[2], color);
  line(pp[2], pp[3], color);
  line(pp[3], pp[0], color);
  line(pp[0], pp[2], color);
  line(pp[1], pp[3], color);
  line(pp[4], pp[5], color);
  line(pp[5], pp[6], color);
  line(pp[6], pp[7], color);
  line(pp[7], pp[4], color);
  line(pp[4], pp[6], color);
  line(pp[5], pp[7], color);
  const auto gridColor = color * 0.7f;
  const int gridLines = 100;
  auto p1 = pp[0];
  auto p2 = pp[1];
  const auto s1 = (pp[4] - pp[0]) / float(gridLines);
  const auto s2 = (pp[5] - pp[1]) / float(gridLines);
  for (int i = 0; i != gridLines; i++, p1 += s1, p2 += s2)
    line(p1, p2, gridColor);
  p1 = pp[2];
  p2 = pp[3];
  const auto s3 = (pp[6] - pp[2]) / float(gridLines);
  const auto s4 = (pp[7] - pp[3]) / float(gridLines);
  for (int i = 0; i != gridLines; i++, p1 += s3, p2 += s4)
    line(p1, p2, gridColor);
  p1 = pp[0];
  p2 = pp[3];
  const auto s5 = (pp[4] - pp[0]) / float(gridLines);
  const auto s6 = (pp[7] - pp[3]) / float(gridLines);
  for (int i = 0; i != gridLines; i++, p1 += s5, p2 += s6)
    line(p1, p2, gridColor);
  p1 = pp[1];
  p2 = pp[2];
  const auto s7 = (pp[5] - pp[1]) / float(gridLines);
  const auto s8 = (pp[6] - pp[2]) / float(gridLines);
  for (int i = 0; i != gridLines; i++, p1 += s7, p2 += s8)
    line(p1, p2, gridColor);
}

void
LineCanvas3D::render(IContext& ctx,
                     const Framebuffer& desc,
                     ICommandBuffer& buf,
                     std::uint32_t num_samples)
{
  if (lines.empty())
    return;

  const auto line_span = std::span(lines);
  const auto required_size = static_cast<std::uint32_t>(line_span.size_bytes());
  if (current_buffer_sizes.at(current_frame) < required_size) {
    lines_buffer.at(current_frame) =
      VulkanDeviceBuffer::create(ctx,
                                 {
                                   .data = std::as_bytes(line_span),
                                   .usage = BufferUsageBits::Storage,
                                   .storage = StorageType::HostVisible,
                                   .size = required_size,
                                   .debug_name = "LineCanvas3D::buffer",
                                 });

    current_buffer_sizes.at(current_frame) = required_size;
  } else {
    auto* mapped_pointer =
      static_cast<LineData*>(ctx.get_buffer_pool()
                               .get(*lines_buffer.at(current_frame))
                               ->allocation_info.pMappedData);
    if (mapped_pointer) {
      std::ranges::copy_n(line_span.data(), line_span.size(), mapped_pointer);
    }
  }

  if (line_pipeline.empty() || num_samples != lines_samples) {
    num_samples = lines_samples;
    line_shader = VulkanShader::create(ctx, "shaders/line_canvas.shader");
    GraphicsPipelineDescription description {
        .topology = Topology::Line,
        .shader = *line_shader,
        .color = {
            ColourAttachment{
                .format = vk_format_to_format(ctx.get_texture_pool().get(desc.color.at(0).texture)->format),
                .blend_enabled = true,
                .src_rgb_blend_factor = BlendFactor::SrcAlpha,
                .dst_rgb_blend_factor = BlendFactor::OneMinusSrcAlpha,
            },
        },
        .depth_format = desc.depth_stencil.texture.valid() ?
              vk_format_to_format(ctx.get_texture_pool().get(desc.depth_stencil.texture)->format):
              Format::Invalid,
        .cull_mode = CullMode::None,
        .polygon_mode = PolygonMode::Line,
        .debug_name = "LineCanvas3D",
      };
    line_pipeline = VulkanGraphicsPipeline::create(ctx, description);
  }

  struct
  {
    glm::mat4 mvp;
    std::uint64_t addr;
  } pc{
    .mvp = this->mvp,
    .addr = ctx.get_buffer_pool()
              .get(*lines_buffer.at(current_frame))
              ->get_device_address(),
  };
  buf.cmd_bind_graphics_pipeline(*line_pipeline);
  buf.cmd_push_constants(pc, 0);

  buf.cmd_draw(static_cast<std::uint32_t>(line_span.size()), 1, 0, 0);
  current_frame = (current_frame + 1) % max_drawables;
}

}