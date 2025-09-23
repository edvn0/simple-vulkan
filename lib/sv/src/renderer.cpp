#include "sv/renderer.hpp"

#include "sv/app.hpp"
#include "sv/camera.hpp"
#include "sv/common.hpp"
#include "sv/context.hpp"
#include "sv/object_handle.hpp"
#include "sv/pipeline.hpp"
#include "sv/shader/shader.hpp"
#include "sv/texture.hpp"
#include "sv/tracing.hpp"
#include "sv/transitions.hpp"
#include "vulkan/vulkan_core.h"

#include <sv/simple-mesh.hpp>

#include <imgui.h>

#include <GLFW/glfw3.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

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

Renderer::Renderer(IContext& ctx,
                   const std::tuple<std::uint32_t, std::uint32_t>& extent)
  : context(&ctx)
  , impl(std::unique_ptr<Renderer::Impl, PimplDeleter>(new Renderer::Impl{},
                                                       PimplDeleter{}))
  , ubo{ ctx, 3 }
  , shadow_ubo{ ctx, 3 }
{
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

  deferred_mrt.shader =
    VulkanShader::create(ctx, "shaders/gbuffer_object.glsl");
  deferred_mrt.pipeline = VulkanGraphicsPipeline::create(
    ctx,
    {
      .vertex_input = vertex_input,
      .shader = *deferred_mrt.shader,
      .color = { ColourAttachment{
        .format = Format::R_UI32,
      }, 
      ColourAttachment {
          .format = Format::A2R10G10B10_UN,
          }
      ,
    ColourAttachment {
          .format = Format::RG_F16,
        }
      ,},
      .depth_format = Format::Z_F32_S_UI8,
      .debug_name = "MRT GBuffer"
    });

  deferred_hdr_gbuffer.shader =
    VulkanShader::create(ctx, "shaders/gbuffer_lighting.glsl");
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

  tonemap.shader = VulkanShader::create(ctx, "shaders/tonemap_hdr_to_sdr.glsl");
  tonemap.pipeline =VulkanGraphicsPipeline::create(
    ctx,
    {
      .shader = *tonemap.shader,
      .color = { ColourAttachment{
        .format = Format::BGRA_UN8,
      } ,},
      .debug_name = "Tonemap"
    });

  grid.shader = VulkanShader::create(*context, "shaders/grid.shader");
  grid.pipeline = VulkanGraphicsPipeline::create(
    *context,
    {
      .shader = *grid.shader,
      .color = { ColourAttachment{
        .format = Format::RGBA_F32,
        .blend_enabled = true,
        .src_rgb_blend_factor = BlendFactor::SrcAlpha,
        .dst_rgb_blend_factor = BlendFactor::OneMinusSrcAlpha,
      } },
      .depth_format = Format::Z_F32_S_UI8,
      .debug_name = "Grid Pipeline",
    });

  directional_shadow.shader =
    VulkanShader::create(*context, "shaders/directional_shadow.shader");
  directional_shadow.pipeline =
    VulkanGraphicsPipeline::create(*context,
                                   {
                                     .vertex_input = vertex_input,
                                     .shader = *directional_shadow.shader,
                                     .depth_format = Format::Z_F32_S_UI8,
                                     .debug_name = "Cascade Shadow Pipeline",
                                   });
  directional_shadow.sampler = VulkanTextureND::create(
    *context,
    VkSamplerCreateInfo{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .mipLodBias = 0.0F,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 1.0F,
      .compareEnable = VK_TRUE,
      .compareOp = VK_COMPARE_OP_GREATER,
      .minLod = 0.0F,
      .maxLod = 4.0F,
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE,
      .unnormalizedCoordinates = VK_FALSE,
    });

  auto&& [w, h] = extent;
  resize(w, h);

  imgui = std::make_unique<ImGuiRenderer>(*context, "fonts/Roboto-Regular.ttf");
}

Renderer::~Renderer() = default;

auto
Renderer::resize(const std::uint32_t width, const std::uint32_t height) -> void
{
  vkDeviceWaitIdle(context->get_device());

  const auto ensure = [this](auto& h, const TextureDescription& d) {
    if (h.valid())
      context->recreate_texture(h, d);
    else
      h = VulkanTextureND::create(*context, d);
  };

  const auto hdr_desc = TextureDescription{
    .format = Format::RGBA_F32,
    .dimensions = { width, height },
    .usage_bits = TextureUsageBits::Sampled | TextureUsageBits::Attachment,
    .debug_name = "GBuffer_Lighting_HDR_RGBA_F32",
  };
  ensure(deferred_hdr_gbuffer.hdr, hdr_desc);

  const auto depth_desc = TextureDescription{
    .format = Format::Z_F32_S_UI8,
    .dimensions = { width, height },
    .usage_bits = TextureUsageBits::Sampled | TextureUsageBits::Attachment,
    .debug_name = "MRT_Depth_F32_S_UI8",
  };
  ensure(deferred_mrt.depth_32, depth_desc);

  const auto material_id_desc = TextureDescription{
    .format = Format::R_UI32,
    .dimensions = { width, height },
    .usage_bits = TextureUsageBits::Sampled | TextureUsageBits::Attachment,
    .debug_name = "MRT_Material_R32",
  };
  ensure(deferred_mrt.material_id, material_id_desc);

  const auto uvs_desc = TextureDescription{
    .format = Format::RG_F16,
    .dimensions = { width, height },
    .usage_bits = TextureUsageBits::Sampled | TextureUsageBits::Attachment,
    .debug_name = "MRT_UVS_RGF16",
  };
  ensure(deferred_mrt.uvs, uvs_desc);

  const auto normals_desc = TextureDescription{
    .format = Format::A2R10G10B10_UN,
    .dimensions = { width, height },
    .usage_bits = TextureUsageBits::Sampled | TextureUsageBits::Attachment,
    .debug_name = "MRT_Normals_A1R5G5B5",
  };
  ensure(deferred_mrt.oct_normals_extras_tbd, normals_desc);

  constexpr auto shadow_map_size = 1 << 12;
  const auto shadow_map_desc = TextureDescription{
    .format = Format::Z_F32_S_UI8,
    .dimensions = { shadow_map_size, shadow_map_size },
    .layer_count = 4,
    .usage_bits = TextureUsageBits::Sampled | TextureUsageBits::Attachment,
    .debug_name = "Directional_Shadow_Map_F32",
  };
  ensure(directional_shadow.texture, shadow_map_desc);

  deferred_extent = { width, height };

  vkDeviceWaitIdle(context->get_device());
}

auto
clear_depth_image(VkCommandBuffer cmd,
                  VkImage image,
                  VkImageLayout current_layout,
                  float depth_value,
                  uint32_t layer_count,
                  uint32_t mip_levels = 1) -> void
{
  VkClearDepthStencilValue clear{};
  clear.depth = depth_value;
  clear.stencil = 0xFF;

  VkImageSubresourceRange range{};
  range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  range.baseMipLevel = 0;
  range.levelCount = mip_levels;
  range.baseArrayLayer = 0;
  range.layerCount = layer_count;

  vkCmdClearDepthStencilImage(
    cmd,
    image,
    current_layout, // must be GENERAL or TRANSFER_DST_OPTIMAL
    &clear,
    1,
    &range);
}

auto
Renderer::record(ICommandBuffer& buf, TextureHandle present) -> void
{
  // Couple of phases.
  // 1: GBuffer static meshes, fully multidraw indirect
  // 1*: Shadows somehow
  // 2: Resolve lighting, sample gbuffer
  // 3: Forward rendering into the resolved lighting
  // 4: Sample resolved lighting in the swapchain

  std::array<glm::mat4, 2> models{};
  {
    ZoneScopedNC("GBuffer", 0xFF00FF);
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
      Framebuffer::AttachmentDescription{deferred_mrt.uvs},
    },
    .depth_stencil = {
          deferred_mrt.depth_32,
    },
    .debug_name = "MRT_GBuffer"
  };

    buf.cmd_begin_rendering(gbuffer_render_pass, gbuffer_framebuffer, {});
    // Here we'll render using an indirect buffer with VkDrawIndirectCommand in
    // the indirect buffer. For now, a super simple cube from a VB & IB
    buf.cmd_bind_graphics_pipeline(*deferred_mrt.pipeline);
    buf.cmd_bind_depth_state({
      .compare_operation = CompareOp::Greater,
      .is_depth_write_enabled = true,
    });
    buf.cmd_bind_vertex_buffer(0, *impl->simple.vertex_buffer, 0);
    buf.cmd_bind_index_buffer(*impl->simple.index_buffer, IndexFormat::UI32, 0);

    auto rotate = glm::rotate(glm::mat4{ 1.0F },
                              static_cast<float>(glfwGetTime()),
                              glm::vec3{ 0.0F, 1.0F, 1.0F });

    models.at(0) = rotate;
    struct PC
    {
      glm::mat4 model;
      std::uint64_t ubo_ref;
      std::uint32_t material_index;
    } pc{
      .model = rotate,
      .ubo_ref = ubo.get(current_frame),
      .material_index = 0,
    };
    models.at(0) = pc.model;
    buf.cmd_push_constants(pc, 0);
    buf.cmd_draw_indexed(impl->simple.index_count, 1, 0, 0, 0);

    pc.model =
      glm::translate(glm::scale(glm::mat4{ 1.0F }, { 100.0F, 1.0F, 100.0F }),
                     glm::vec3{ 0, 3, 0 });
    models.at(1) = pc.model;

    buf.cmd_push_constants(pc, 0);
    buf.cmd_draw_indexed(impl->simple.index_count, 1, 0, 0, 0);
    buf.cmd_end_rendering();
  }

  {
    ZoneScopedNC("Directional shadow pass", 0x0F0F0F);

    struct PC
    {
      std::uint64_t ubo{};
      glm::mat4 model{};
    };

    const RenderPass shadow_rp {
        .depth = {
          .load_op = LoadOp::Clear,
          .store_op = StoreOp::Store,
          .layer  =1,
          .clear_depth = 0.0F,
        },
    };
    const Framebuffer shadow_fb{
      .depth_stencil =
        Framebuffer::AttachmentDescription{
          *directional_shadow.texture,
        },
    };
    ImageTransition::transition_layout(
      buf.get_command_buffer(),
      context->get_texture_pool().get(*directional_shadow.texture)->image,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_GENERAL,
      VkImageSubresourceRange{
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
      });
    clear_depth_image(
      buf.get_command_buffer(),
      context->get_texture_pool().get(*directional_shadow.texture)->image,
      VK_IMAGE_LAYOUT_GENERAL,
      0.0F,
      context->get_texture_pool()
        .get(*directional_shadow.texture)
        ->layer_count);
    buf.cmd_begin_rendering(shadow_rp, shadow_fb, {});

    {
      buf.cmd_bind_graphics_pipeline(*directional_shadow.pipeline);
      buf.cmd_bind_depth_state({
        .compare_operation = CompareOp::Greater,
        .is_depth_write_enabled = true,
      });
      buf.cmd_bind_vertex_buffer(0, *impl->simple.vertex_buffer, 0);
      buf.cmd_bind_index_buffer(
        *impl->simple.index_buffer, IndexFormat::UI32, 0);

      PC pc{
        .ubo = shadow_ubo.get(current_frame),
        .model = models.at(0),
      };
      buf.cmd_push_constants(pc, 0);
      buf.cmd_draw_indexed(impl->simple.index_count, 1, 0, 0, 0);

      pc.model = models.at(1);
      buf.cmd_push_constants(pc, 0);
      buf.cmd_draw_indexed(impl->simple.index_count, 1, 0, 0, 0);
    }

    buf.cmd_end_rendering();
  }

  {
    ZoneScopedNC("GBuffer Resolve", 0x00FFFF);
    buf.cmd_begin_rendering(
      { .color = { RenderPass::AttachmentDescription{
          .load_op = LoadOp::Clear,
          .store_op = StoreOp::Store,
          .clear_colour = std::array<float, 4>{ 0, 0, 0, 0 },
        } } },
      { .color = { Framebuffer::AttachmentDescription{
          *deferred_hdr_gbuffer.hdr, }, }, },
      {});
    buf.cmd_bind_graphics_pipeline(*deferred_hdr_gbuffer.pipeline);
    struct PC
    {
      std::uint32_t normals_tex;
      std::uint32_t depth_tex;
      std::uint32_t material_tex;
      std::uint32_t uvs_tex;
      std::uint32_t sampler_id;

      std::uint32_t shadow_tex;
      std::uint32_t shadow_sampler_id;
      std::uint32_t shadow_layers;
      std::uint64_t ubo;
    } pc{
      deferred_mrt.oct_normals_extras_tbd.index(),
      deferred_mrt.depth_32.index(),
      deferred_mrt.material_id.index(),
      deferred_mrt.uvs.index(),
      0,
      directional_shadow.texture.index(),
      directional_shadow.sampler.index(),
      4,
      ubo.get(current_frame),
    };
    buf.cmd_bind_depth_state({
      .compare_operation = CompareOp::AlwaysPass,
    });
    buf.cmd_push_constants(pc, 0);
    buf.cmd_draw(3, 1, 0, 0);
    buf.cmd_end_rendering();
  }

  {
    ZoneScopedNC("Forward pass", 0x22FF22);
    RenderPass forward_pass{
    .color = {
        RenderPass::AttachmentDescription{
            .load_op = LoadOp::Load,
            .store_op = StoreOp::Store,
        },
    },
    .depth = {.load_op = LoadOp::Load, .store_op = StoreOp::DontCare,},
    .stencil = {},
    .layer_count = 1,
    .view_mask = 0,
    };

    Framebuffer forward_framebuffer{
    .color = {
        Framebuffer::AttachmentDescription{  *deferred_hdr_gbuffer.hdr },
    },
    .depth_stencil = {
      *deferred_mrt.depth_32
    },
    .debug_name = "Forward FB",
};
    buf.cmd_begin_rendering(forward_pass, forward_framebuffer, {});
    buf.cmd_bind_graphics_pipeline(*grid.pipeline);
    buf.cmd_bind_depth_state({
      .compare_operation = CompareOp::Greater,
    });
    struct GridPC
    {
      std::uint64_t ubo_address; // matches UBO pc
      std::uint64_t padding{ 0 };
      alignas(16) glm::vec4 origin;
      alignas(16) glm::vec4 grid_colour_thin;
      alignas(16) glm::vec4 grid_colour_thick;
      alignas(16) glm::vec4 grid_params;
    };
    GridPC grid_pc{
      .ubo_address = ubo.get(current_frame),
      .origin = glm::vec4{ 0.0f },
      .grid_colour_thin = glm::vec4{ 0.5f, 0.5f, 0.5f, 1.0f },
      .grid_colour_thick = glm::vec4{ 0.15f, 0.15f, 0.15f, 1.0f },
      .grid_params = glm::vec4{ 100.0f, 0.025f, 2.0f, 0.0f },
    };
    buf.cmd_push_constants<GridPC>(grid_pc, 0);
    buf.cmd_draw(6, 1, 0, 0);

    canvas_3d.clear();

    canvas_3d.box(glm::translate(glm::mat4{ 1.0F }, glm::vec3{ 5, 5, 0 }),
                  BoundingBox(glm::vec3(-2), glm::vec3(+2)),
                  glm::vec4(1, 1, 0, 1));
    static auto initial_pos = -8.F;
    auto&& [w, h] = deferred_extent;
    canvas_3d.frustum(
      glm::lookAt(
        glm::vec3(cos(glfwGetTime()), initial_pos, sin(glfwGetTime())),
        glm::vec3{ 0, 7, -4 },
        glm::vec3(0.0f, 1.0f, 0.0f)),
      glm::perspective(
        glm::radians(60.0f), static_cast<float>(w) / h, 10.0f, 30.0f),
      glm::vec4(1, 1, 1, 1));
    canvas_3d.render(*context, forward_framebuffer, buf, 1);

    buf.cmd_end_rendering();
  }

  {
    const RenderPass tonemap_render_pass{
      .color{
        RenderPass::AttachmentDescription{
          .load_op = LoadOp::Clear,
          .store_op = StoreOp::Store,
          .clear_colour = { std::array<float, 4>{ 0, 0, 0, 0 } },
        },
      },
    };

    const Framebuffer tonemap_framebuffer{
      .color = { Framebuffer::AttachmentDescription{ present } },
      .debug_name = "Swapchain_Tonemap"
    };

    buf.cmd_begin_rendering(tonemap_render_pass, tonemap_framebuffer, {});
    buf.cmd_bind_graphics_pipeline(*tonemap.pipeline);
    buf.cmd_bind_depth_state({});
    const struct TonemapPC
    {
      std::uint32_t hdr_tex;
      std::uint32_t sampler_id;
      float exposure;
    } tonemap_pc{
      .hdr_tex = deferred_hdr_gbuffer.hdr.index(),
      .sampler_id = 0,
      .exposure = 1.0F,
    };
    buf.cmd_push_constants(tonemap_pc, 0);
    buf.cmd_draw(3, 1, 0, 0);
    buf.cmd_end_rendering();
  }

  {
    const RenderPass ui_render_pass{
      .color{
        RenderPass::AttachmentDescription{
          .load_op = LoadOp::Load,
          .store_op = StoreOp::Store,
        },
      },
      .depth = {
                .load_op = LoadOp::Load, 
                .store_op = StoreOp::DontCare,
            },
    };

    const Framebuffer ui_framebuffer{
      .color = { Framebuffer::AttachmentDescription{ present } },
      .depth_stencil = { Framebuffer::AttachmentDescription{
        deferred_mrt.depth_32 } },
      .debug_name = "Swapchain_UI"
    };

    buf.cmd_begin_rendering(ui_render_pass, ui_framebuffer, {});

    imgui->begin_frame(ui_framebuffer);
    ImGui::Begin("Light direction");
    ImGui::SliderAngle("Light Direction (phi)",
                       &rad_phi,
                       0.0F,
                       360.0F,
                       "%.1f",
                       ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderAngle("Light Direction (theta)",
                       &rad_theta,
                       -180.0F,
                       180.0F,
                       "%.1f",
                       ImGuiSliderFlags_AlwaysClamp);
    ImGui::End();
    imgui->end_frame(buf);

    buf.cmd_end_rendering();
  }

  current_frame++;
}

static auto
orthonormal_basis(const glm::vec3& d) -> std::pair<glm::vec3, glm::vec3>
{
  const glm::vec3 up =
    std::abs(d.y) > 0.99f ? glm::vec3{ 0, 0, 1 } : glm::vec3{ 0, 1, 0 };
  const glm::vec3 right = glm::normalize(glm::cross(up, d));
  const glm::vec3 new_up = glm::normalize(glm::cross(d, right));
  return { right, new_up };
}

auto
Renderer::build_centered_cascades(const glm::vec3& light_dir,
                                  const ShadowSplits& splits,
                                  float z_near,
                                  float z_far) -> ShadowUBOData
{
  ShadowUBOData out{};
  out.cascade_count = splits.count;

  const glm::vec3 dir = glm::normalize(light_dir);
  auto [right, up] = orthonormal_basis(dir);
  const glm::vec3 center{ 0.f };
  const float depth = 0.5f * (z_near + z_far);
  const glm::vec3 eye = center - dir * depth;

  const glm::mat4 view = glm::lookAt(eye, center, up);

  for (std::uint32_t i = 0; i < splits.count; ++i) {
    const float e = splits.half_extents[i];
    const glm::mat4 proj = glm::ortho(-e, e, -e, e, 0.0f, z_near + z_far);
    out.cascades[i].view = view;
    out.cascades[i].proj = proj;
    out.cascades[i].vp = proj * view;
  }
  return out;
}

auto
Renderer::update_shadow_ubo_layers(const glm::vec3& light_dir) -> void
{
  ShadowUBOData s =
    build_centered_cascades(light_dir, shadow_splits, shadow_near, shadow_far);
  shadow_ubo.upload(current_frame, s);
}

auto
Renderer::begin_frame(const Camera& camera) -> void
{
  glm::vec3 dir{};

  dir.x = glm::cos(rad_phi) * glm::cos(rad_theta);
  dir.y = glm::sin(rad_phi);
  dir.z = glm::cos(rad_phi) * glm::sin(rad_theta);
  dir = -glm::normalize(dir);

  auto&& [w, h] = deferred_extent;
  const auto aspect = static_cast<float>(w) / static_cast<float>(h);
  const auto proj =
    glm::perspective(glm::radians(70.0F), aspect, 0.01F, 1000.0F);
  auto constructed_ubo = this->create_ubo(camera.get_view_matrix(), proj);
  constructed_ubo.light_direction = glm::vec4(dir, 0.0f);
  constructed_ubo.camera_position = glm::vec4(camera.get_position(), 1.0F);
  ubo.upload(current_frame, constructed_ubo);

  update_shadow_ubo_layers(dir);

  canvas_3d.set_mvp(proj * camera.get_view_matrix());
}

}