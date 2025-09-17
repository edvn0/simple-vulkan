#pragma once

#include "sv/abstract_command_buffer.hpp"

namespace sv {

class CommandBuffer final : public ICommandBuffer
{
  static constexpr Dependencies empty_deps{};

public:
  CommandBuffer() = default;
  explicit CommandBuffer(IContext&);
  ~CommandBuffer() override;

  [[nodiscard]] auto get_command_buffer() const
  {
    return wrapper->command_buffer;
  }

  auto cmd_begin_rendering(const RenderPass& render_pass,
                           const Framebuffer& framebuffer,
                           const Dependencies& deps) -> void override;
  auto cmd_end_rendering() -> void override;
  auto cmd_bind_viewport(const Viewport& viewport) -> void override;
  auto cmd_bind_scissor_rect(const ScissorRect& rect) -> void override;
  auto cmd_bind_graphics_pipeline(GraphicsPipelineHandle handle)
    -> void override;
  auto cmd_bind_compute_pipeline(ComputePipelineHandle handle) -> void override;
  auto cmd_bind_depth_state(const DepthState& state) -> void override;
  auto cmd_draw(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t)
    -> void override;
  auto cmd_draw_indexed(std::uint32_t,
                        std::uint32_t,
                        std::uint32_t,
                        std::int32_t,
                        std::uint32_t) -> void override;
  auto cmd_draw_indexed_indirect(BufferHandle,
                                 std::size_t,
                                 std::uint32_t,
                                 std::uint32_t) -> void override;
  auto cmd_dispatch_thread_groups(const Dimensions&) -> void override;
  auto cmd_push_constants(std::span<const std::byte>) -> void override;
  auto cmd_bind_index_buffer(BufferHandle index_buffer,
                             IndexFormat index_format,
                             std::uint64_t index_buffer_offset)
    -> void override;
  auto cmd_bind_vertex_buffer(std::uint32_t index,
                              BufferHandle buffer,
                              std::uint64_t buffer_offset) -> void override;

private:
  VulkanContext* context{ nullptr };
  const CommandBufferWrapper* wrapper{ nullptr };

  Framebuffer framebuffer = {};
  SubmitHandle last_submit_handle = {};

  VkPipeline last_pipeline_bound = VK_NULL_HANDLE;

  bool is_rendering = false;
  std::uint32_t view_mask = 0;

  GraphicsPipelineHandle current_pipeline_graphics = {};
  ComputePipelineHandle current_pipeline_compute = {};

  friend class VulkanContext;
};
}