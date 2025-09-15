#include "sv/context.hpp"

#include "sv/app.hpp"
#include "sv/common.hpp"
#include "sv/renderer.hpp"

#include <GLFW/glfw3.h>
#include <iostream>

namespace sv {

VulkanContext::~VulkanContext()
{
  while (!delete_queue.empty()) {
    auto& back = delete_queue.back();
    back(*this);
    delete_queue.pop_back();
  }

  vkb::destroy_swapchain(swapchain);
  vkDestroySurfaceKHR(instance, surface, nullptr);
  vkb::destroy_device(device);
  vkb::destroy_instance(instance);
}

auto
VulkanContext::create(const Window& window)
  -> std::expected<std::unique_ptr<IContext>, ContextError>
{
  using enum ContextError::Code;
  auto ptr = static_cast<GLFWwindow*>(window.opaque_handle);
  if (!ptr)
    return make_error<ContextError>(InvalidWindow, "Window not initialised");

  vkb::InstanceBuilder instance_builder;
  auto instance_ret = instance_builder.require_api_version(1, 4)
                        .use_default_debug_messenger()
                        .request_validation_layers()
                        .build();
  if (!instance_ret) {
    std::cout << instance_ret.error().message() << "\n";
    const auto& detailed_reasons = instance_ret.detailed_failure_reasons();
    if (!detailed_reasons.empty()) {
      std::cerr << "Instance Selection failure reasons:\n";
      for (const std::string& reason : detailed_reasons) {
        std::cerr << reason << "\n";
      }
    }
    return make_error<ContextError>(InvalidWindow, "blah");
  }
  auto instance = instance_ret.value();

  auto inst_disp = instance.make_table();

  vkb::PhysicalDeviceSelector phys_device_selector(instance);

  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkResult err = glfwCreateWindowSurface(instance, ptr, nullptr, &surface);
  if (err) {
    const char* error_msg;
    int ret = glfwGetError(&error_msg);
    if (ret != 0) {
      std::cout << ret << " ";
      if (error_msg != nullptr)
        std::cout << error_msg;
      std::cout << "\n";
    }
    surface = VK_NULL_HANDLE;
  }

  VkPhysicalDeviceFeatures required_features{};
  required_features.multiViewport = true;
  required_features.multiDrawIndirect = true;
  required_features.inheritedQueries = true;

  VkPhysicalDeviceVulkan11Features required_11_features{};
  required_11_features.shaderDrawParameters = true;

  VkPhysicalDeviceVulkan12Features required_12_features{};
  required_12_features.bufferDeviceAddress = true;
  required_12_features.descriptorIndexing = true;
  required_12_features.timelineSemaphore = true;

  VkPhysicalDeviceVulkan13Features required_13_features{};
  required_13_features.dynamicRendering = true;
  required_13_features.synchronization2 = true;

  auto phys_device_builder = phys_device_selector.set_minimum_version(1, 3)
                               .set_surface(surface)
                               .set_required_features(required_features)
                               .set_required_features_11(required_11_features)
                               .set_required_features_12(required_12_features)
                               .set_required_features_13(required_13_features);

  auto phys_device_ret = phys_device_builder.select();
  if (!phys_device_ret) {
    std::cout << phys_device_ret.error().message() << "\n";
    if (phys_device_ret.error() ==
        vkb::PhysicalDeviceError::no_suitable_device) {
      const auto& detailed_reasons = phys_device_ret.detailed_failure_reasons();
      if (!detailed_reasons.empty()) {
        std::cerr << "GPU Selection failure reasons:\n";
        for (const std::string& reason : detailed_reasons) {
          std::cerr << reason << "\n";
        }
      }
    }
    return make_error<ContextError>(InvalidWindow, "blah");
  }
  vkb::PhysicalDevice physical_device = phys_device_ret.value();

  vkb::DeviceBuilder device_builder{ physical_device };

  auto device_ret = device_builder.build();
  if (!device_ret) {
    std::cout << device_ret.error().message() << "\n";
    return make_error<ContextError>(InvalidWindow, "blah");
  }
  auto device = device_ret.value();

  auto disp = device.make_table();

  vkb::SwapchainBuilder swapchain_builder{ device };
  auto swap_ret =
    swapchain_builder
      .set_desired_min_image_count(vkb::SwapchainBuilder::DOUBLE_BUFFERING)
      .build();
  if (!swap_ret) {
    std::cout << swap_ret.error().message() << " " << swap_ret.vk_result()
              << "\n";
    return make_error(ContextError{ InvalidWindow, "blah" });
  }
  auto swapchain = swap_ret.value();

  auto gfam = device.get_queue_index(vkb::QueueType::graphics).value();
  auto pfam = device.get_queue_index(vkb::QueueType::present).value();
  VkQueue gq{}, pq{};
  vkGetDeviceQueue(device, gfam, 0, &gq);
  vkGetDeviceQueue(device, pfam, 0, &pq);

  using P = std::unique_ptr<IContext>;
  auto vk_context = new VulkanContext(std::move(instance),
                                      std::move(device),
                                      std::move(swapchain),
                                      std::move(disp),
                                      std::move(inst_disp),
                                      surface,
                                      gq,
                                      gfam,
                                      pq,
                                      pfam);
  vk_context->surface = surface;
  auto context_ptr = P();
  context_ptr.reset(vk_context);
  return std::expected<P, ContextError>{ std::in_place,
                                         std::move(context_ptr) };
}

}