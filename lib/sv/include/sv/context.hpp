#pragma once

#include "sv/app.hpp"
#include "sv/bindless_access.hpp"
#include "sv/buffer.hpp"
#include "sv/immediate_commands.hpp"
#include "sv/object_handle.hpp"
#include "sv/object_pool.hpp"
#include "sv/staging_allocator.hpp"
#include "sv/texture.hpp"
#include "vulkan/vulkan_core.h"

#include <VkBootstrap.h>
#include <deque>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace sv {

struct Window;

class DeviceAllocator
{
private:
  static inline VmaAllocator allocator{ VK_NULL_HANDLE };

public:
  static auto initialise(IContext&) -> void;
  static auto deinitialise() -> void { vmaDestroyAllocator(the()); }
  static auto the() -> VmaAllocator&;
};

struct ContextError
{
  enum class Code : std::uint8_t
  {
    None,
    InvalidWindow,
  };

  Code code{ Code::None };
  std::string message;
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

  virtual auto destroy(GraphicsPipelineHandle) -> void = 0;
  virtual auto destroy(ComputePipelineHandle) -> void = 0;

  virtual auto get_buffer_pool() -> BufferPool& = 0;
  virtual auto destroy(BufferHandle) -> void = 0;

  struct OffsetSize
  {
    const VkDeviceSize offset;
    const VkDeviceSize size;
  };
  virtual auto flush_mapped_memory(BufferHandle, OffsetSize) const -> void = 0;
  virtual auto invalidate_mapped_memory(BufferHandle, OffsetSize) const
    -> void = 0;
};

class VulkanContext final : public IContext
{
  vkb::Instance instance;
  vkb::Device device;
  vkb::DispatchTable dispatch_table;
  vkb::InstanceDispatchTable instance_dispatch_table;
  struct VulkanProperties
  {
    VkPhysicalDeviceProperties base{};
    VkPhysicalDeviceVulkan11Properties eleven{};
    VkPhysicalDeviceVulkan12Properties twelve{};
    VkPhysicalDeviceVulkan13Properties thirteen{};
    VkPhysicalDeviceVulkan14Properties fourteen{};
  };
  VulkanProperties vulkan_properties{};
  VkSurfaceKHR surface{ nullptr };
  VkQueue graphics_queue{};
  VkQueue present_queue{};
  std::uint32_t graphics_family{};
  std::uint32_t present_family{};

  TexturePool textures;
  BufferPool buffers;
  DescriptorArrays descriptors;
  friend struct BindlessAccess<VulkanContext>;
  bool needs_descriptor_update{ true };

  std::unique_ptr<StagingAllocator> staging_allocator;
  friend class StagingAllocator;

  std::unique_ptr<ImmediateCommands> immediate_commands;
  friend class ImmediateCommands;

  Holder<TextureHandle> dummy_texture;
  // Holder<TextureHandle> dummy_sampler;

  std::deque<std::function<void(IContext&)>> delete_queue;
  std::deque<std::function<void(IContext&)>> pre_frame_queue;

  auto create_placeholder_resources() -> void;

  VulkanContext(vkb::Instance&& i,
                vkb::Device&& d,
                vkb::DispatchTable&& dt,
                vkb::InstanceDispatchTable&& idt,
                VkSurfaceKHR surf,
                VkQueue gq,
                std::uint32_t gfam,
                VkQueue pq,
                std::uint32_t pfam);

public:
  ~VulkanContext();
  [[nodiscard]] auto get_instance() const -> VkInstance override
  {
    return instance;
  }
  [[nodiscard]] auto get_physical_device() const -> VkPhysicalDevice override
  {
    return device.physical_device;
  }
  [[nodiscard]] auto get_device() const -> VkDevice override { return device; }
  [[nodiscard]] auto get_device_wrapper() const -> const vkb::Device& override
  {
    return device;
  }
  [[nodiscard]] auto get_graphics_queue() const -> VkQueue override
  {
    return graphics_queue;
  }
  [[nodiscard]] auto get_present_queue() const -> VkQueue override
  {
    return present_queue;
  }
  [[nodiscard]] auto get_graphics_queue_family() const -> std::uint32_t override
  {
    return graphics_family;
  }
  [[nodiscard]] auto get_present_queue_family() const -> std::uint32_t override
  {
    return present_family;
  }
  [[nodiscard]] auto get_surface() const -> VkSurfaceKHR override
  {
    return surface;
  }
  auto enqueue_destruction(std::function<void(IContext&)>&& f) -> void override
  {
    delete_queue.emplace_back(std::forward<std::function<void(IContext&)>>(f));
  }
  auto defer_task(std::function<void(IContext&)>&& f) -> void override
  {
    pre_frame_queue.emplace_back(
      std::forward<std::function<void(IContext&)>>(f));
  }
  auto initialise_resources() -> void override
  {
    staging_allocator = std::make_unique<StagingAllocator>(*this);
    immediate_commands = std::make_unique<ImmediateCommands>(*this, "Debug?");
    create_placeholder_resources();
  }
  auto update_resources() -> void override { needs_descriptor_update = true; }

  auto get_texture_pool() -> TexturePool& override { return textures; }
  auto get_texture_pool() const -> const TexturePool& { return textures; }
  auto destroy(TextureHandle) -> void override;

  auto destroy(GraphicsPipelineHandle) -> void override;
  auto destroy(ComputePipelineHandle) -> void override;

  auto get_buffer_pool() -> BufferPool& override { return buffers; }
  auto get_buffer_pool() const -> const BufferPool& { return buffers; }
  auto destroy(BufferHandle) -> void override;

  auto flush_mapped_memory(BufferHandle, OffsetSize) const -> void override;
  auto invalidate_mapped_memory(BufferHandle, OffsetSize) const
    -> void override;

  static auto create(const Window&)
    -> std::expected<std::unique_ptr<IContext>, ContextError>;
};

template<>
struct BindlessAccess<VulkanContext>
{
  static auto device(VulkanContext& c) -> VkDevice { return c.get_device(); }
  static auto descriptors(VulkanContext& c) -> DescriptorArrays&
  {
    return c.descriptors;
  }
  static auto textures(VulkanContext& c) -> TexturePool& { return c.textures; }
  static auto needs_descriptor_update(VulkanContext& c) -> bool&
  {
    return c.needs_descriptor_update;
  }
  static auto enqueue_destruction(VulkanContext& c,
                                  std::function<void(IContext& context)>&& f)
  {
    c.enqueue_destruction(std::move(f));
  }
  static auto defer_task(VulkanContext& c,
                         std::function<void(IContext& context)>&& f)
  {
    c.defer_task(std::move(f));
  }
  static auto process_pre_frame_work(VulkanContext& c)
  {
    if (c.pre_frame_queue.empty())
      return;

    auto m = std::move(c.pre_frame_queue.back());
    c.pre_frame_queue.pop_back();
    m(c);
  }
};

}