#pragma once

#include "sv/common.hpp"
#include "sv/immediate_commands.hpp"

namespace sv {

class VulkanContext;

struct ICommandBuffer
{
public:
  virtual ~ICommandBuffer() = default;
  /*
    virtual auto transition_to_shader_read_only(TextureHandle surface) const
        -> void = 0;

    virtual auto
    cmd_push_debug_group_label(const char *label,
                               std::uint32_t color_rgba = 0xffffffff) const
        -> void = 0;
    virtual auto
    cmd_insert_debug_event_label(const char *label,
                                 std::uint32_t color_rgba = 0xffffffff) const
        -> void = 0;
    virtual auto cmd_pop_debug_group_label() const -> void = 0;

    virtual auto cmd_bind_compute_pipeline(ComputePipelineHandle handle)
        -> void = 0;
    virtual auto cmd_dispatch_thread_groups(const Dimensions &threadgroup_count,
                                            const Dependencies &deps = {})
        -> void = 0;
*/
  virtual auto get_command_buffer() const -> VkCommandBuffer = 0;
  virtual auto cmd_begin_rendering(const RenderPass& render_pass,
                                   const Framebuffer& framebuffer,
                                   const Dependencies& deps) -> void = 0;
  virtual auto cmd_end_rendering() -> void = 0;

  virtual auto cmd_bind_viewport(const Viewport& viewport) -> void = 0;
  virtual auto cmd_bind_scissor_rect(const ScissorRect& rect) -> void = 0;

  virtual auto cmd_bind_graphics_pipeline(GraphicsPipelineHandle) -> void{};
  virtual auto cmd_bind_compute_pipeline(ComputePipelineHandle) -> void{};
  virtual auto cmd_bind_depth_state(const DepthState& state) -> void = 0;
  virtual auto cmd_draw(std::uint32_t vertex_count,
                        std::uint32_t instance_count,
                        std::uint32_t first_vertex,
                        std::uint32_t base_instance) -> void = 0;
  virtual auto cmd_draw_indexed(std::uint32_t index_count,
                                std::uint32_t instance_count,
                                std::uint32_t first_index,
                                std::int32_t vertex_offset,
                                std::uint32_t base_instance) -> void = 0;
  virtual auto cmd_draw_indexed_indirect(BufferHandle indirect_buffer,
                                         std::size_t indirect_buffer_offset,
                                         std::uint32_t draw_count,
                                         std::uint32_t stride) -> void = 0;
  virtual auto cmd_dispatch_thread_groups(const Dimensions&) -> void = 0;

  virtual auto cmd_push_constants(std::span<const std::byte>) -> void{};
  template<typename T>
  auto cmd_push_constants(const T& data, std::size_t offset) -> void
  {
    this->cmd_push_constants(std::span<const std::byte>{
      reinterpret_cast<const std::byte*>(&data) + offset,
      sizeof(T) - offset,
    });
  }

  virtual auto cmd_bind_index_buffer(BufferHandle index_buffer,
                                     IndexFormat index_format,
                                     std::uint64_t index_buffer_offset)
    -> void = 0;
  virtual auto cmd_bind_vertex_buffer(std::uint32_t index,
                                      BufferHandle buffer,
                                      std::uint64_t buffer_offset) -> void = 0;

  /*


          virtual auto cmd_fill_buffer(BufferHandle buffer, std::size_t
         buffer_offset, std::size_t size, std::uint32_t data)
              -> void = 0;
          virtual auto cmd_update_buffer(BufferHandle buffer, std::size_t
          buffer_offset, std::size_t size, const void *data)
              -> void = 0;
          template <typename Struct>
          auto cmd_update_buffer(BufferHandle buffer, const Struct &data,
                                 std::size_t buffer_offset = 0) -> void {
            this->cmd_update_buffer(buffer, buffer_offset, sizeof(Struct),
     &data);
          }


          virtual auto cmd_draw_indirect(BufferHandle indirect_buffer,
                                         std::size_t indirect_buffer_offset,
                                         std::uint32_t draw_count,
                                         std::uint32_t stride = 0) -> void = 0;

       -> void = 0; virtual auto cmd_draw_indexed_indirect_count( BufferHandle
          indirect_buffer, std::size_t indirect_buffer_offset, BufferHandle
          count_buffer, std::size_t count_buffer_offset, std::uint32_t
         max_draw_count, std::uint32_t stride = 0) -> void = 0;

          virtual auto cmd_draw_mesh_tasks(const Dimensions &threadgroup_count)
              -> void = 0;
          virtual auto cmd_draw_mesh_tasks_indirect(BufferHandle
     indirect_buffer, std::size_t indirect_buffer_offset, std::uint32_t
     draw_count, std::uint32_t stride = 0)
              -> void = 0;
          virtual auto cmd_draw_mesh_tasks_indirect_count(
              BufferHandle indirect_buffer, std::size_t indirect_buffer_offset,
              BufferHandle count_buffer, std::size_t count_buffer_offset,
              std::uint32_t max_draw_count, std::uint32_t stride = 0) -> void =
     0;

          virtual auto cmd_trace_rays(std::uint32_t width, std::uint32_t height,
                                      std::uint32_t depth = 1,
                                      const Dependencies &deps = {}) -> void =
     0;

          virtual auto cmd_set_blend_color(const float color[4]) -> void = 0;
          virtual auto cmd_set_depth_bias(float constant_factor, float
       slope_factor, float clamp = 0.0f) -> void = 0; virtual auto
       cmd_set_depth_bias_enable(bool enable) -> void = 0;

          virtual auto cmd_reset_query_pool(QueryPoolHandle pool,
                                            std::uint32_t first_query,
                                            std::uint32_t query_count) -> void =
       0; virtual auto cmd_write_timestamp(QueryPoolHandle pool, std::uint32_t
         query)
              -> void = 0;

          virtual auto cmd_clear_color_image(TextureHandle tex,
                                             const ClearColourValue &value,
                                             const TextureLayers &layers = {})
              -> void = 0;
          virtual auto cmd_copy_image(TextureHandle src, TextureHandle dst,
                                      const Dimensions &extent,
                                      const Offset3D &src_offset = {},
                                      const Offset3D &dst_offset = {},
                                      const TextureLayers &src_layers = {},
                                      const TextureLayers &dst_layers = {}) ->
       void = 0; virtual auto cmd_generate_mipmap(TextureHandle handle) -> void
     = 0;
          */
};

}