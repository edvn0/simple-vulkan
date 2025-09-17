#pragma once

#include "sv/abstract_command_buffer.hpp"
#include "sv/abstract_context.hpp"
#include "sv/app.hpp"
#include "sv/bindless_access.hpp"
#include "sv/buffer.hpp"
#include "sv/command_buffer.hpp"
#include "sv/immediate_commands.hpp"
#include "sv/object_handle.hpp"
#include "sv/object_pool.hpp"
#include "sv/staging_allocator.hpp"

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
  static auto initialise(VkInstance, VkPhysicalDevice, VkDevice) -> void;
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

namespace detail {
template<auto Member, class... Args>
auto
dispatch_member(const vkb::DispatchTable& tbl, Args&&... args)
  -> std::invoke_result_t<decltype(std::declval<const vkb::DispatchTable&>().*
                                   Member),
                          Args...>
{
  using fn_ptr_t = decltype(std::declval<const vkb::DispatchTable&>().*Member);
  static_assert(!std::is_same_v<fn_ptr_t, void*>,
                "Function not available in this build.");

  auto fn = tbl.*Member;
  using ret_t = std::invoke_result_t<fn_ptr_t, Args...>;

  if (!fn) {
    if constexpr (std::is_same_v<ret_t, VkResult>)
      return VK_ERROR_EXTENSION_NOT_PRESENT;
    else
      throw std::runtime_error("Vulkan function not loaded.");
  }

  if constexpr (std::is_same_v<ret_t, void>)
    return fn(std::forward<Args>(args)...);
  else
    return fn(std::forward<Args>(args)...);
}
}

class VulkanContext;
class VulkanSwapchain final
{
  static constexpr auto max_image_count = 16U;

public:
  VulkanSwapchain(IContext&);
  ~VulkanSwapchain();

  auto present(VkSemaphore) -> bool;

  auto get_current_image() const -> VkImage;
  auto get_current_image_view() const -> VkImageView;
  auto get_current_texture() -> TextureHandle;
  auto get_current_image_index() const -> std::uint32_t {
    return current_image_index;
  }
  auto get_surface_format() const { return swapchain.image_format; }
  auto get_image_count() const { return swapchain.image_count; }

  auto resize(std::uint32_t, std::uint32_t) -> void;

public:
  VulkanContext* context{ nullptr };
  vkb::Swapchain swapchain;
  uint32_t current_image_index = 0; // [0...numSwapchainImages_)
  uint64_t current_frame_index = 0; // [0...+inf)
  bool get_next_image = true;
  std::array<TextureHandle , max_image_count> swapchain_textures {};
  std::array<VkSemaphore , max_image_count> acquire_semaphores {};
  std::array<VkFence , max_image_count> present_fence {};
  std::array<uint64_t, max_image_count> timeline_wait_values{};

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

  static inline ContextConfiguration config;

  TexturePool textures;
  BufferPool buffers;
  GraphicsPipelinePool graphics_pipelines;
  ComputePipelinePool compute_pipelines;
  ShaderModulePool shader_modules;

  DescriptorArrays descriptors;
  friend struct BindlessAccess<VulkanContext>;
  bool needs_descriptor_update{ true };

  std::unique_ptr<StagingAllocator> staging_allocator;
  friend class StagingAllocator;

  std::unique_ptr<ImmediateCommands> immediate_commands;
  friend class ImmediateCommands;

  Holder<TextureHandle> dummy_texture;
  // Holder<TextureHandle> dummy_sampler;

  CommandBuffer command_buffer;
  friend class CommandBuffer;

  std::unique_ptr<VulkanSwapchain> swapchain;
  VkSemaphore timeline_semaphore;
  bool has_swapchain_maintenance_1{ false };
  friend class VulkanSwapchain;

  std::deque<std::function<void(IContext&)>> delete_queue;
  std::deque<std::function<void(IContext&)>> pre_frame_queue;

  auto create_placeholder_resources() -> void;
  auto initialise_swapchain(std::uint32_t, std::uint32_t) -> bool;

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
  auto update_resources() -> void override { needs_descriptor_update = true; }

  auto get_texture_pool() -> TexturePool& override { return textures; }
  auto get_texture_pool() const -> const TexturePool& { return textures; }
  auto destroy(TextureHandle) -> void override;

  auto get_graphics_pipeline_pool() -> GraphicsPipelinePool& override
  {
    return graphics_pipelines;
  }
  auto get_graphics_pipeline_pool() const -> const GraphicsPipelinePool&
  {
    return graphics_pipelines;
  }
  auto destroy(GraphicsPipelineHandle) -> void override;

  auto get_compute_pipeline_pool() -> ComputePipelinePool& override
  {
    return compute_pipelines;
  }
  auto get_compute_pipeline_pool() const -> const ComputePipelinePool&
  {
    return compute_pipelines;
  }
  auto destroy(ComputePipelineHandle) -> void override;

  auto get_shader_module_pool() -> ShaderModulePool& override
  {
    return shader_modules;
  }
  auto get_shader_module_pool() const -> const ShaderModulePool&
  {
    return shader_modules;
  }
  auto destroy(ShaderModuleHandle) -> void override;

  auto get_buffer_pool() -> BufferPool& override { return buffers; }
  auto get_buffer_pool() const -> const BufferPool& { return buffers; }
  auto destroy(BufferHandle) -> void override;

  auto flush_mapped_memory(BufferHandle, OffsetSize) const -> void override;
  auto invalidate_mapped_memory(BufferHandle, OffsetSize) const
    -> void override;

  auto acquire_command_buffer() -> ICommandBuffer& override
  {
    command_buffer = CommandBuffer{ *this };
    return command_buffer;
  }

  auto recreate_swapchain(std::uint32_t w, std::uint32_t h) -> bool {
    return initialise_swapchain(w, h);
  }
  auto get_current_swapchain_texture() -> TextureHandle override;

auto submit(ICommandBuffer& commandBuffer, TextureHandle present)
    -> SubmitHandle override;

  auto get_immediate_commands() -> ImmediateCommands& override
  {
    return *immediate_commands;
  }
  auto get_staging_allocator() -> StagingAllocator& override
  {
    return *staging_allocator;
  }

  auto get_pipeline(ComputePipelineHandle) -> VkPipeline;
  auto get_pipeline(GraphicsPipelineHandle) -> VkPipeline;

  template<auto Member, class... Args>
  auto dispatch(Args&&... args) const
    -> std::invoke_result_t<decltype(std::declval<const vkb::DispatchTable&>().*
                                     Member),
                            Args...>
  {
    return detail::dispatch_member<Member>(dispatch_table,
                                           std::forward<Args>(args)...);
  }

  auto bind_default_descriptor_sets(const VkCommandBuffer cmd,
                                    const VkPipelineBindPoint bind_point,
                                    const VkPipelineLayout layout) const -> void
  {
    const std::array dsets{
      descriptors.set,
    };
    vkCmdBindDescriptorSets(cmd,
                            bind_point,
                            layout,
                            0,
                            static_cast<std::uint32_t>(dsets.size()),
                            dsets.data(),
                            0,
                            nullptr);
  }

  static auto create(const Window&, const ContextConfiguration& = {})
    -> std::expected<std::unique_ptr<IContext>, ContextError>;
};
#define VKB_MEMBER(name) &vkb::DispatchTable::fp_##name

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