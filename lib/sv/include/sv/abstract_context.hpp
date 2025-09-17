#pragma once

#include <VkBootstrap.h>
#include <vulkan/vulkan.h>

#include "sv/abstract_command_buffer.hpp"
#include "sv/object_handle.hpp"
#include "sv/object_pool.hpp"

namespace sv {

class ImmediateCommands;
class StagingAllocator;

struct ContextConfiguration
{
  bool abort_on_validation_error{ false };
};

struct IContext
{
  virtual ~IContext() = default;
  [[nodiscard]] virtual auto get_instance() const -> VkInstance = 0;
  [[nodiscard]] virtual auto get_physical_device() const
    -> VkPhysicalDevice = 0;
  [[nodiscard]] virtual auto get_device() const -> VkDevice = 0;
  [[nodiscard]] virtual auto get_device_wrapper() const
    -> const vkb::Device& = 0;
  [[nodiscard]] virtual auto get_graphics_queue() const -> VkQueue = 0;
  [[nodiscard]] virtual auto get_present_queue() const -> VkQueue = 0;
  [[nodiscard]] virtual auto get_graphics_queue_family() const
    -> std::uint32_t = 0;
  [[nodiscard]] virtual auto get_present_queue_family() const
    -> std::uint32_t = 0;
  [[nodiscard]] virtual auto get_surface() const -> VkSurfaceKHR = 0;
  virtual auto initialise_resources() -> void {};
  virtual auto update_resources() -> void = 0;

  virtual auto enqueue_destruction(std::function<void(IContext&)>&& f)
    -> void = 0;
  virtual auto defer_task(std::function<void(IContext&)>&& f) -> void = 0;

  virtual auto get_texture_pool() -> TexturePool& = 0;
  virtual auto destroy(TextureHandle) -> void = 0;

  virtual auto get_graphics_pipeline_pool() -> GraphicsPipelinePool& = 0;
  virtual auto destroy(GraphicsPipelineHandle) -> void = 0;

  virtual auto get_compute_pipeline_pool() -> ComputePipelinePool& = 0;
  virtual auto destroy(ComputePipelineHandle) -> void = 0;

  virtual auto get_shader_module_pool() -> ShaderModulePool& = 0;
  virtual auto destroy(ShaderModuleHandle) -> void = 0;

  virtual auto get_buffer_pool() -> BufferPool& = 0;
  virtual auto destroy(BufferHandle) -> void = 0;

  virtual auto acquire_command_buffer() -> ICommandBuffer& = 0;
  virtual auto submit(ICommandBuffer&, TextureHandle) -> void = 0;

  struct OffsetSize
  {
    const VkDeviceSize offset;
    const VkDeviceSize size;
  };
  virtual auto flush_mapped_memory(BufferHandle, OffsetSize) const -> void = 0;
  virtual auto invalidate_mapped_memory(BufferHandle, OffsetSize) const
    -> void = 0;

  virtual auto get_immediate_commands() -> ImmediateCommands& = 0;
  virtual auto get_staging_allocator() -> StagingAllocator& = 0;
};

}