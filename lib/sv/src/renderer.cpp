#include "sv/renderer.hpp"

#include "sv/app.hpp"
#include "sv/common.hpp"
#include "sv/object_handle.hpp"
#include "sv/shader/shader.hpp"
#include "sv/transitions.hpp"
#include "vulkan/vulkan_core.h"

#include "sv/simple-mesh.hpp"

#include <GLFW/glfw3.h>
#include <glm/ext/matrix_clip_space.hpp>

namespace sv {

/*
namespace {
auto
full_range_color(VkImage image) -> VkImageMemoryBarrier2
{
  VkImageMemoryBarrier2 b{};
  b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  b.image = image;
  b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  b.subresourceRange.baseMipLevel = 0;
  b.subresourceRange.levelCount = 1;
  b.subresourceRange.baseArrayLayer = 0;
  b.subresourceRange.layerCount = 1;
  return b;
}

auto
to_general(VkCommandBuffer buf, VkImage image, VkImageLayout old_layout) -> void
{
  VkImageMemoryBarrier2 b = full_range_color(image);
  b.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
  b.srcAccessMask = 0;
  b.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  b.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  b.oldLayout = old_layout;
  b.newLayout = VK_IMAGE_LAYOUT_GENERAL;

  VkDependencyInfo dep{};
  dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dep.imageMemoryBarrierCount = 1;
  dep.pImageMemoryBarriers = &b;
  vkCmdPipelineBarrier2(buf, &dep);
}

auto
to_present(VkCommandBuffer buf, VkImage image) -> void
{
  VkImageMemoryBarrier2 b = full_range_color(image);
  b.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  b.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  b.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
  b.dstAccessMask = 0;
  b.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  b.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkDependencyInfo dep{};
  dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dep.imageMemoryBarrierCount = 1;
  dep.pImageMemoryBarriers = &b;
  vkCmdPipelineBarrier2(buf, &dep);
}

auto
begin_record(VkCommandBuffer cmd, const sv::AcquiredFrame& af) -> void
{
  to_general(cmd, af.image, VK_IMAGE_LAYOUT_UNDEFINED);

  VkRenderingAttachmentInfo color{};
  color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color.imageView = af.view;
  color.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.clearValue.color = { .float32 = { 1.0F, 0.F, 0.F, 1.F } };

  VkRenderingInfo ri{};
  ri.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  ri.renderArea = VkRect2D{ { 0, 0 }, af.extent };
  ri.layerCount = 1;
  ri.colorAttachmentCount = 1;
  ri.pColorAttachments = &color;

  vkCmdBeginRendering(cmd, &ri);
}

auto
end_record(VkCommandBuffer cmd, const sv::AcquiredFrame& af) -> void
{
  vkCmdEndRendering(cmd);
  to_present(cmd, af.image);
}
}
*/

struct Renderer::Impl
{
  simple::SimpleGeometryMesh simple;
};

Renderer::Renderer(IContext& ctx, const std::tuple<std::uint32_t, std::uint32_t>& extent)
  : context(&ctx)
  , impl(std::unique_ptr<Renderer::Impl, PimplDeleter>(new Renderer::Impl{}, PimplDeleter{}))
{
  basic_shader = VulkanShader::create(ctx, "shaders/simple.glsl");
  basic = VulkanGraphicsPipeline::create(
    ctx,
    {
      .shader = *basic_shader,
      .color = { ColourAttachment{
        .format = Format::BGRA_UN8,
      } ,},
      .debug_name = "Basic"
    });

  impl->simple = simple::SimpleGeometryMesh::create(
    *context,
    simple::SimpleGeometryParams{ 
          .kind = simple::SimpleGeometryParams::Kind::Cube,
          .half_extents = glm::vec3{ 5, 5, 5 },
          .debug_name = "Cube",
  });

  const auto vertex_input = VertexInput::create({
    VertexFormat::Float3,
    VertexFormat::Float3,
    VertexFormat::Float2,
  });

  deferred_mrt.shader = VulkanShader::create(ctx, "shaders/gbuffer_object.glsl");
  deferred_mrt.pipeline = VulkanGraphicsPipeline::create(
    ctx,
    {
      .vertex_input = vertex_input,
      .shader = *deferred_mrt.shader,
      .color = { ColourAttachment{
        .format = Format::R_UI32,
      }, ColourAttachment {
          .format = Format::A2R10G10B10_UN,
}
      
      ,},
      .depth_format = Format::Z_F32_S_UI8,
      .debug_name = "MRT GBuffer"
    });

  deferred_hdr_gbuffer.shader =
    VulkanShader::create(ctx, "shaders/lighting_gbuffer.glsl");
  deferred_hdr_gbuffer.pipeline =
  VulkanGraphicsPipeline::create(
    ctx,
    {
      .shader = *deferred_hdr_gbuffer.shader,
      .color = { ColourAttachment{
        .format = Format::RGBA_F32,
      } ,},
      .debug_name = "Lighting GBuffer"
    });


  auto&& [w, h] = extent;
  resize(w, h);
}

auto
Renderer::resize(const std::uint32_t width, const std::uint32_t height) -> void
{
  deferred_hdr_gbuffer.hdr = 
    VulkanTextureND::create(
    *context,
    TextureDescription{
      .format = Format::RGBA_F32,
      .dimensions = { width, height },
      .usage_bits = TextureUsageBits::Sampled | TextureUsageBits::Attachment,
      .debug_name = "GBuffer_Lighting_HDR_RGBA_F32",
    });

    deferred_mrt.depth_32 = VulkanTextureND::create(
    *context,
    TextureDescription{
      .format = Format::Z_F32_S_UI8,
      .dimensions = { width, height },
      .usage_bits = TextureUsageBits::Sampled | TextureUsageBits::Attachment,
      .debug_name = "MRT_Depth_F32_S_UI8",
    });

  deferred_mrt.material_id = VulkanTextureND::create(
    *context,
    TextureDescription{
      .format = Format::R_UI32,
      .dimensions = { width, height },
      .usage_bits = TextureUsageBits::Sampled | TextureUsageBits::Attachment,
      .debug_name = "MRT_Material_R32",
    });

  deferred_mrt.oct_normals_extras_tbd = VulkanTextureND::create(
      *context,
      TextureDescription{
        .format = Format::A2R10G10B10_UN,
        .dimensions = { width, height },
        .usage_bits = TextureUsageBits::Sampled | TextureUsageBits::Attachment,
        .debug_name = "MRT_Normals_A1R5G5B5",
      });

    deferred_extent = { width, height };
}

auto
Renderer::record(ICommandBuffer& buf, TextureHandle present) -> void
{
  const RenderPass gbuffer_render_pass{
    .color{
      RenderPass::AttachmentDescription{
        .load_op = LoadOp::Clear,
        .store_op = StoreOp::Store,
        .clear_colour = { std::array<float, 4>{ 0, 0, 0, 0 } },
      },
      RenderPass::AttachmentDescription{
        .load_op = LoadOp::Clear,
        .store_op = StoreOp::Store,
        .clear_colour = { std::array<float, 4>{ 0, 0, 0, 0 } },
      },
    },
      .depth =
        RenderPass::AttachmentDescription{
          .load_op = LoadOp::Clear,
          .store_op = StoreOp::Store,
          .clear_depth = 0.0F, // Reverse Z
        },
  };

  const Framebuffer gbuffer_framebuffer{
    .color = {
      Framebuffer::AttachmentDescription{deferred_mrt.material_id},
      Framebuffer::AttachmentDescription{deferred_mrt.oct_normals_extras_tbd},
    },
    .depth_stencil = {
          deferred_mrt.depth_32,
    },
    .debug_name = "MRT_GBuffer"
  };

  buf.cmd_begin_rendering(gbuffer_render_pass, gbuffer_framebuffer, {});
  // Here we'll render using an indirect buffer with VkDrawIndirectCommand in the indirect buffer.
  // For now, a super simple cube from a VB & IB
  buf.cmd_bind_graphics_pipeline(*deferred_mrt.pipeline);
  buf.cmd_bind_depth_state({
      .compare_operation = CompareOp::Greater, 
      .is_depth_write_enabled = true,
  });
  buf.cmd_bind_vertex_buffer(0, *impl->simple.vertex_buffer, 0);
  buf.cmd_bind_index_buffer(*impl->simple.index_buffer, IndexFormat::UI32, 0);
  buf.cmd_draw_indexed(impl->simple.index_count, 1, 0, 0, 0);
  buf.cmd_end_rendering();


  const RenderPass basic_render_pass {
    .color {
      RenderPass::AttachmentDescription{
        .load_op = LoadOp::Clear,
        .store_op = StoreOp::Store,
        .clear_colour = {std::array<float, 4>{0,0,0,0}}
      },
    },
  };

  const Framebuffer basic_framebuffer { 
      .color = { Framebuffer::AttachmentDescription{
                   present,
                 } },
                 .debug_name = "Swapchain" };

  buf.cmd_begin_rendering(basic_render_pass, basic_framebuffer, {});
  buf.cmd_bind_graphics_pipeline(*basic);
  buf.cmd_bind_depth_state({});
  auto size = context->get_texture_pool().get(present)->extent;

  auto projection = glm::ortho<float>(0.F,1.F, 0, 1.F, 1.0F, 0.0F);
  auto view = glm::mat4{ 1.0F };

  const struct Time
  {
    float t{ static_cast<float>(glfwGetTime()) };
    std::uint32_t tex_index{ 0 };
    glm::mat4 mvp;
  } pc
  {
    .mvp = projection * view
  };
  buf.cmd_push_constants(pc, 0);
  buf.cmd_draw(3, 1, 0, 0);
  buf.cmd_end_rendering();
}

}