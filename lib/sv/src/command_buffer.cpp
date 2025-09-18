#include "sv/command_buffer.hpp"

#include "sv/bindless.hpp"
#include "sv/context.hpp"
#include "sv/transitions.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vulkan/vulkan_core.h>

namespace sv {

namespace {

constexpr auto sample_count_more_than_one = [](VkSampleCountFlagBits sc) {
  switch (sc) {
    case VK_SAMPLE_COUNT_1_BIT:
      return false;
    case VK_SAMPLE_COUNT_2_BIT:
    case VK_SAMPLE_COUNT_4_BIT:
    case VK_SAMPLE_COUNT_8_BIT:
    case VK_SAMPLE_COUNT_16_BIT:
    case VK_SAMPLE_COUNT_32_BIT:
    case VK_SAMPLE_COUNT_64_BIT:
      return true;
    default:
      return false;
  }
};

auto
set_clear_colour(VkClearColorValue& dst, const ClearColourValue& src) -> void
{
  static constexpr auto for_each = [](auto& dst_array, const auto& src_array) {
    for (auto i = 0U; i < std::size(src_array); ++i) {
      dst_array[i] = src_array[i];
    }
  };

  std::visit(
    [&]<typename T>(const T& value) {
      if constexpr (std::is_same_v<T, std::array<float, 4>>) {
        for_each(dst.float32, value);
      } else if constexpr (std::is_same_v<T, std::array<std::uint32_t, 4>>) {
        for_each(dst.uint32, value);
      } else if constexpr (std::is_same_v<T, std::array<std::int32_t, 4>>) {
        for_each(dst.int32, value);
      }
    },
    src);
}

static constexpr auto
resolve_mode_to_vk_resolve_mode_flag_bits(ResolveMode mode,
                                          VkResolveModeFlags supported)
  -> VkResolveModeFlagBits
{
  switch (mode) {
    case ResolveMode::None:
      return VK_RESOLVE_MODE_NONE;
    case ResolveMode::SampleZero:
      return VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
    case ResolveMode::Average:
      return (supported & VK_RESOLVE_MODE_AVERAGE_BIT)
               ? VK_RESOLVE_MODE_AVERAGE_BIT
               : VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
    case ResolveMode::Min:
      return (supported & VK_RESOLVE_MODE_MIN_BIT)
               ? VK_RESOLVE_MODE_MIN_BIT
               : VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
    case ResolveMode::Max:
      return (supported & VK_RESOLVE_MODE_MAX_BIT)
               ? VK_RESOLVE_MODE_MAX_BIT
               : VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
  }
  assert(false);
  return VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
}

static constexpr auto load_op_to_vk_attachment_load_op =
  [](LoadOp op) -> VkAttachmentLoadOp {
  switch (op) {
    case LoadOp::Load:
      return VK_ATTACHMENT_LOAD_OP_LOAD;
    case LoadOp::Clear:
      return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case LoadOp::DontCare:
      return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    case LoadOp::None:
      return VK_ATTACHMENT_LOAD_OP_NONE;
    default:
      return VK_ATTACHMENT_LOAD_OP_MAX_ENUM;
  }
};

static constexpr auto store_op_to_vk_attachment_store_op =
  [](StoreOp op) -> VkAttachmentStoreOp {
  switch (op) {
    case StoreOp::Store:
      return VK_ATTACHMENT_STORE_OP_STORE;
    case StoreOp::MsaaResolve:
    case StoreOp::DontCare:
      return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    case StoreOp::None:
      return VK_ATTACHMENT_STORE_OP_NONE;
    default:
      return VK_ATTACHMENT_STORE_OP_MAX_ENUM;
  }
};

} // namespace

CommandBuffer::~CommandBuffer()
{
  assert(!is_rendering);
}

CommandBuffer::CommandBuffer(IContext& ctx)
  : context(dynamic_cast<VulkanContext*>(&ctx))
  , wrapper(&context->get_immediate_commands().acquire())
{
}

auto
CommandBuffer::cmd_begin_rendering(const RenderPass& render_pass,
                                   const Framebuffer& fb,
                                   const Dependencies& deps) -> void
{
  assert(!is_rendering);

  is_rendering = true;
  view_mask = render_pass.view_mask;

  for (std::uint32_t i = 0;
       i != Dependencies::max_dependencies && deps.textures[i];
       i++) {
    auto* image = context->get_texture_pool().get(deps.textures[i]);
    Transition::image(wrapper->command_buffer,
                      image->image,
                      VK_IMAGE_LAYOUT_UNDEFINED,
                      VK_IMAGE_LAYOUT_GENERAL);
  }

  const std::uint32_t framebuffer_colour_attachment_count =
    fb.get_colour_attachment_count();

  framebuffer = fb;

  for (std::uint32_t i = 0; i != framebuffer_colour_attachment_count; i++) {
    if (const auto& handle = fb.color[i].texture; handle) {
      auto* texture = context->get_texture_pool().get(handle);
      Transition::image(wrapper->command_buffer,
                        texture->image,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_GENERAL);
    }
    if (const auto& handle = fb.color[i].resolve_texture; handle) {
      auto* color_resolve_texture = context->get_texture_pool().get(handle);
      Transition::image(wrapper->command_buffer,
                        color_resolve_texture->image,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_GENERAL);
    }
  }

  TextureHandle depth_texture = fb.depth_stencil.texture;

  VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
  std::uint32_t mip_level = 0;
  std::uint32_t framebuffer_width = 0;
  std::uint32_t framebuffer_height = 0;

  std::array<VkRenderingAttachmentInfo, max_colour_attachments>
    colour_attachments{};

  for (std::uint32_t i = 0; i != framebuffer_colour_attachment_count; i++) {
    auto&& [texture, resolve_texture] = fb.color[i];
    assert(!texture.empty());

    auto* color_texture = context->get_texture_pool().get(texture);
    const auto& desc_color = render_pass.color[i];
    if (mip_level && desc_color.level) {
      assert(desc_color.level == mip_level &&
             "All color attachments should have the same mip-level");
    }
    const VkExtent3D& dim = color_texture->extent;
    if (framebuffer_width) {
      assert(dim.width == framebuffer_width &&
             "All attachments should have the same width");
    }
    if (framebuffer_height) {
      assert(dim.height == framebuffer_height &&
             "All attachments should have the same height");
    }

    mip_level = desc_color.level;
    framebuffer_width = dim.width;
    framebuffer_height = dim.height;
    samples = color_texture->samples;
    colour_attachments[i] = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .pNext = nullptr,
      .imageView = color_texture->get_or_create_image_view_for_framebuffer(
        *context, desc_color.level, desc_color.layer),
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
      .resolveMode =
        sample_count_more_than_one(samples)
          ? resolve_mode_to_vk_resolve_mode_flag_bits(
              desc_color.resolve_mode, VK_RESOLVE_MODE_FLAG_BITS_MAX_ENUM)
          : VK_RESOLVE_MODE_NONE,
      .resolveImageView = VK_NULL_HANDLE,
      .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .loadOp = load_op_to_vk_attachment_load_op(desc_color.load_op),
      .storeOp = store_op_to_vk_attachment_store_op(desc_color.store_op),
      .clearValue = { .color = { .float32 = { 0, 0, 0, 0 } } },
    };
    set_clear_colour(colour_attachments.at(i).clearValue.color,
                     desc_color.clear_colour);

    if (desc_color.store_op == StoreOp::MsaaResolve) {
      assert(samples > 1);
      assert(!resolve_texture.empty() &&
             "Framebuffer attachment should contain a resolve texture");
      auto* colour_resolve_texture =
        context->get_texture_pool().get(resolve_texture);
      colour_attachments[i].resolveImageView =
        colour_resolve_texture->get_or_create_image_view_for_framebuffer(
          *context, desc_color.level, desc_color.layer);
      colour_attachments[i].resolveImageLayout = VK_IMAGE_LAYOUT_GENERAL;
    }
  }

  VkRenderingAttachmentInfo depth_attachment = {};

  if (fb.depth_stencil.texture) {
    auto* depth_texture_obj =
      context->get_texture_pool().get(fb.depth_stencil.texture);
    const auto& desc_depth = render_pass.depth;
    assert(
      desc_depth.level == mip_level &&
      "Depth attachment should have the same mip-level as color attachments");
    depth_attachment = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .pNext = nullptr,
      .imageView = depth_texture_obj->get_or_create_image_view_for_framebuffer(
        *context, desc_depth.level, desc_depth.layer),
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
      .resolveMode = VK_RESOLVE_MODE_NONE,
      .resolveImageView = VK_NULL_HANDLE,
      .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .loadOp = load_op_to_vk_attachment_load_op(desc_depth.load_op),
      .storeOp = store_op_to_vk_attachment_store_op(desc_depth.store_op),
      .clearValue = { .depthStencil = { .depth = desc_depth.clear_depth,
                                        .stencil = desc_depth.clear_stencil } },
    };
    if (desc_depth.store_op == StoreOp::MsaaResolve) {
      assert(depth_texture_obj->samples == samples);
      const auto& attachment = fb.depth_stencil;
      assert(!attachment.resolve_texture.empty() &&
             "Framebuffer depth attachment should contain a resolve texture");
      auto* depth_resolve_texture =
        context->get_texture_pool().get(attachment.resolve_texture);
      depth_attachment.resolveImageView =
        depth_resolve_texture->get_or_create_image_view_for_framebuffer(
          *context, desc_depth.level, desc_depth.layer);
      depth_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_GENERAL;
      depth_attachment.resolveMode = resolve_mode_to_vk_resolve_mode_flag_bits(
        desc_depth.resolve_mode,
        context->vulkan_properties.twelve.supportedDepthResolveModes);
    }
    const auto& dim = depth_texture_obj->extent;
    if (framebuffer_width) {
      assert(dim.width == framebuffer_width &&
             "All attachments should have the same width");
    }
    if (framebuffer_height) {
      assert(dim.height == framebuffer_height &&
             "All attachments should have the same height");
    }
    mip_level = desc_depth.level;
    framebuffer_width = dim.width;
    framebuffer_height = dim.height;
  }

  const std::uint32_t width = std::max(framebuffer_width >> mip_level, 1u);
  const std::uint32_t height = std::max(framebuffer_height >> mip_level, 1u);
  const Viewport viewport = {
    0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height),
    1.0f, 0.0f,
  };
  const ScissorRect scissor = { 0, 0, width, height };

  VkRenderingAttachmentInfo stencil_attachment = depth_attachment;
  const bool is_stencil_format = render_pass.stencil.load_op != LoadOp::Invalid;

  const VkRenderingInfo rendering_info = {
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .pNext = nullptr,
    .flags = 0,
    .renderArea = { { static_cast<std::int32_t>(scissor.x),
                                static_cast<std::int32_t>(scissor.y), },
                    { scissor.width, scissor.height, }, },
    .layerCount = render_pass.layer_count,
    .viewMask = view_mask,
    .colorAttachmentCount = framebuffer_colour_attachment_count,
    .pColorAttachments = colour_attachments.data(),
    .pDepthAttachment = depth_texture ? &depth_attachment : nullptr,
    .pStencilAttachment = is_stencil_format ? &stencil_attachment : nullptr,
  };

  const VkViewport vp = {
    .x = viewport.x,
    .y = viewport.height,
    .width = viewport.width,
    .height = -viewport.height,
    .minDepth = viewport.minDepth,
    .maxDepth = viewport.maxDepth,
  };
  vkCmdSetViewport(wrapper->command_buffer, 0, 1, &vp);

  VkRect2D rect = { .offset = { static_cast<std::int32_t>(scissor.x),
                                static_cast<std::int32_t>(scissor.y), },
                    .extent = { scissor.width, scissor.height, }, };
  vkCmdSetScissor(wrapper->command_buffer, 0, 1, &rect);

  Bindless<VulkanContext>::sync_on_frame_acquire(*context);

  vkCmdBeginRendering(wrapper->command_buffer, &rendering_info);
}

auto
CommandBuffer::cmd_end_rendering() -> void
{
  vkCmdEndRendering(wrapper->command_buffer);
  is_rendering = false;
  framebuffer = {};
}

auto
CommandBuffer::cmd_bind_viewport(const Viewport& viewport) -> void
{
  assert(is_rendering && "Viewport can only be bound during rendering");
  const VkViewport vp = {
    .x = viewport.x,
    .y = viewport.height,
    .width = viewport.width,
    .height = -viewport.height,
    .minDepth = viewport.minDepth,
    .maxDepth = viewport.maxDepth,
  };
  vkCmdSetViewport(wrapper->command_buffer, 0, 1, &vp);
}

auto
CommandBuffer::cmd_bind_scissor_rect(const ScissorRect& rect) -> void
{
  assert(is_rendering && "Scissor rect can only be bound during rendering");
  VkRect2D vk_rect = { .offset = { static_cast<std::int32_t>(rect.x),
                                   static_cast<std::int32_t>(rect.y), },
                       .extent = { rect.width, rect.height, }, };
  vkCmdSetScissor(wrapper->command_buffer, 0, 1, &vk_rect);
}

auto
CommandBuffer::cmd_bind_depth_state(const DepthState& state) -> void
{
  assert(is_rendering && "Depth state can only be bound during rendering");
  vkCmdSetDepthTestEnable(wrapper->command_buffer,
                          state.is_depth_test_enabled ? VK_TRUE : VK_FALSE);
  vkCmdSetDepthCompareOp(wrapper->command_buffer,
                         static_cast<VkCompareOp>(state.compare_operation));
  vkCmdSetDepthWriteEnable(wrapper->command_buffer,
                           state.is_depth_write_enabled ? VK_TRUE : VK_FALSE);
  vkCmdSetDepthBiasEnable(wrapper->command_buffer, VK_FALSE);
}

auto
CommandBuffer::cmd_draw(std::uint32_t vertex_count,
                        std::uint32_t instance_count,
                        std::uint32_t first_vertex,
                        std::uint32_t base_instance) -> void
{
  assert(is_rendering && "Draw can only be called during rendering");
  vkCmdDraw(wrapper->command_buffer,
            vertex_count,
            instance_count,
            first_vertex,
            base_instance);
}

auto
CommandBuffer::cmd_draw_indexed(std::uint32_t index_count,
                                std::uint32_t instance_count,
                                std::uint32_t first_index,
                                std::int32_t vertex_offset,
                                std::uint32_t base_instance) -> void
{
  assert(is_rendering && "Draw indexed can only be called during rendering");
  vkCmdDrawIndexed(wrapper->command_buffer,
                   index_count,
                   instance_count,
                   first_index,
                   vertex_offset,
                   base_instance);
}

auto
CommandBuffer::cmd_draw_indexed_indirect(BufferHandle indirect_buffer,
                                         size_t indirect_buffer_offset,
                                         uint32_t draw_count,
                                         uint32_t stride) -> void
{
  auto* indirect = context->get_buffer_pool().get(indirect_buffer);

  vkCmdDrawIndexedIndirect(wrapper->command_buffer,
                           indirect->get_buffer(),
                           indirect_buffer_offset,
                           draw_count,
                           stride ? stride
                                  : sizeof(VkDrawIndexedIndirectCommand));
}

auto
CommandBuffer::cmd_dispatch_thread_groups(const Dimensions& xyz) -> void
{
  const auto x = std::max(xyz.width, 1u);
  const auto y = std::max(xyz.height, 1u);
  const auto z = std::max(xyz.depth, 1u);
  vkCmdDispatch(wrapper->command_buffer, x, y, z);
}

auto
CommandBuffer::cmd_bind_compute_pipeline(ComputePipelineHandle handle) -> void
{
  if (handle.empty()) {
    return;
  }

  current_pipeline_compute = handle;

  const auto* pipeline = context->get_compute_pipeline_pool().get(handle);

  assert(pipeline);

  const auto vk_pipeline = context->get_pipeline(handle);

  assert(vk_pipeline != VK_NULL_HANDLE);

  if (last_pipeline_bound != vk_pipeline) {
    last_pipeline_bound = vk_pipeline;
    vkCmdBindPipeline(
      wrapper->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk_pipeline);
    context->bind_default_descriptor_sets(wrapper->command_buffer,
                                          VK_PIPELINE_BIND_POINT_COMPUTE,
                                          pipeline->get_layout());
  }
}

auto
CommandBuffer::cmd_bind_graphics_pipeline(const GraphicsPipelineHandle handle)
  -> void
{
  if (handle.empty()) {
    return;
  }

  current_pipeline_graphics = handle;

  const auto* pipeline = context->get_graphics_pipeline_pool().get(handle);

  assert(pipeline);

  const bool has_depth_attachment_pipeline =
    pipeline->description.depth_format != Format::Invalid;
  const bool has_depth_attachment_pass =
    !framebuffer.depth_stencil.texture.empty();

  if (has_depth_attachment_pipeline != has_depth_attachment_pass) {
    assert(false);
  }

  const auto vk_pipeline = context->get_pipeline(handle /*viewMask??*/);

  assert(vk_pipeline != VK_NULL_HANDLE);

  if (last_pipeline_bound != vk_pipeline) {
    last_pipeline_bound = vk_pipeline;
    vkCmdBindPipeline(
      wrapper->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline);
    context->bind_default_descriptor_sets(wrapper->command_buffer,
                                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                                          pipeline->get_layout());
  }
}

auto
CommandBuffer::cmd_push_constants(const std::span<const std::byte> data) -> void
{
  const auto device_limits =
    context->vulkan_properties.base.limits.maxPushConstantsSize;
  if (data.empty() || data.size_bytes() % 4 != 0 ||
      data.size_bytes() > device_limits) {
    std::cerr << "Push constants must be a multiple of 4 bytes." << std::endl;
    return;
  }

  if (current_pipeline_compute.empty() && current_pipeline_graphics.empty()) {
    std::cerr << "No pipeline bound for push constants." << std::endl;
    return;
  }

  const auto* graphics_pipeline =
    context->get_graphics_pipeline_pool().get(current_pipeline_graphics);
  const auto* compute_pipeline =
    context->get_compute_pipeline_pool().get(current_pipeline_compute);

  assert(graphics_pipeline || compute_pipeline);

  const VkPipelineLayout pipeline_layout = graphics_pipeline
                                             ? graphics_pipeline->get_layout()
                                             : compute_pipeline->get_layout();
  const VkShaderStageFlags stage_flags =
    graphics_pipeline ? graphics_pipeline->get_stage_flags()
                      : compute_pipeline->get_stage_flags();

  if (pipeline_layout == VK_NULL_HANDLE) {
    std::cerr << "Pipeline layout is null for push constants." << std::endl;
    return;
  }

  vkCmdPushConstants(
    wrapper->command_buffer,
    pipeline_layout,
    stage_flags,
    0,
    static_cast<std::uint32_t>(get_aligned_size(data.size_bytes(), 4)),
    data.data());
}

auto
CommandBuffer::cmd_bind_index_buffer(BufferHandle index_buffer,
                                     IndexFormat index_format,
                                     std::uint64_t index_buffer_offset) -> void
{
  assert(is_rendering && "Index buffer can only be bound during rendering");
  if (index_buffer.empty()) {
    return;
  }

  const auto* buffer = context->get_buffer_pool().get(index_buffer);
  if (!buffer) {
    std::cerr << "Invalid index buffer handle." << std::endl;
    return;
  }

  vkCmdBindIndexBuffer(wrapper->command_buffer,
                       buffer->get_buffer(),
                       index_buffer_offset,
                       static_cast<VkIndexType>(index_format));
}

void
CommandBuffer::cmd_bind_vertex_buffer(const std::uint32_t index,
                                      const BufferHandle vertex_buffer,
                                      const std::uint64_t buffer_offset)
{
  const auto* buffer = context->get_buffer_pool().get(vertex_buffer);

  const std::array buffers{ buffer->get_buffer() };
  vkCmdBindVertexBuffers2(wrapper->command_buffer,
                          index,
                          static_cast<std::uint32_t>(buffers.size()),
                          buffers.data(),
                          &buffer_offset,
                          nullptr,
                          nullptr);
}

}
