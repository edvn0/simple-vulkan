#pragma once

#include <VkBootstrap.h>
#include <deque>
#include <expected>
#include <functional>
#include <memory>
#include <vulkan/vulkan.h>

namespace sv {

struct Window;

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
  [[nodiscard]] virtual auto get_graphics_queue_family() const -> uint32_t = 0;
  [[nodiscard]] virtual auto get_present_queue_family() const -> uint32_t = 0;
  [[nodiscard]] virtual auto get_surface() const -> VkSurfaceKHR = 0;

  virtual auto enqueue_destruction(std::function<void(IContext&)>&& f)
    -> void = 0;
};

class VulkanContext : IContext
{
  vkb::Instance instance;
  vkb::Device device;
  vkb::Swapchain swapchain;
  vkb::DispatchTable dispatch_table;
  vkb::InstanceDispatchTable instance_dispatch_table;
  VkSurfaceKHR surface{ nullptr };
  VkQueue graphics_queue{};
  VkQueue present_queue{};
  uint32_t graphics_family{};
  uint32_t present_family{};

  std::deque<std::function<void(IContext&)>> delete_queue;

  VulkanContext(vkb::Instance&& i,
                vkb::Device&& d,
                vkb::Swapchain&& sc,
                vkb::DispatchTable&& dt,
                vkb::InstanceDispatchTable&& idt,
                VkSurfaceKHR surf,
                VkQueue gq,
                std::uint32_t gfam,
                VkQueue pq,
                std::uint32_t pfam)
    : instance(std::move(i))
    , device(std::move(d))
    , swapchain(std::move(sc))
    , dispatch_table(std::move(dt))
    , instance_dispatch_table(std::move(idt))
    , surface(surf)
    , graphics_queue(gq)
    , graphics_family(gfam)
    , present_queue(pq)
    , present_family(pfam)
  {
  }

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
  [[nodiscard]] auto get_graphics_queue_family() const -> uint32_t override
  {
    return graphics_family;
  }
  [[nodiscard]] auto get_present_queue_family() const -> uint32_t override
  {
    return present_family;
  }
  [[nodiscard]] auto get_surface() const -> VkSurfaceKHR override
  {
    return surface;
  }
  auto enqueue_destruction(std::function<void(IContext&)>&& f) -> void
  {
    delete_queue.emplace_back(std::forward<std::function<void(IContext&)>>(f));
  }

  static auto create(const Window&)
    -> std::expected<std::unique_ptr<IContext>, ContextError>;
};

}