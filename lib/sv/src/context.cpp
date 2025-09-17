#include "sv/context.hpp"

#include "sv/object_handle.hpp"
#include "sv/texture.hpp"

#include "sv/app.hpp"
#include "sv/common.hpp"
#include "sv/renderer.hpp"
#include "sv/scope_exit.hpp"

#include <GLFW/glfw3.h>
#include <cstdlib>
#include <iostream>
#include <span>

namespace sv {

namespace {
VKAPI_ATTR VkBool32 VKAPI_CALL
vk_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                  VkDebugUtilsMessageTypeFlagsEXT messageType,
                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                  void*)
{
  auto ms = vkb::to_string_message_severity(messageSeverity);
  auto mt = vkb::to_string_message_type(messageType);

  static auto& out = std::cerr;

  if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
    out << std::format("[{}: {}] - {}\n{}\n",
                       ms,
                       mt,
                       pCallbackData->pMessageIdName,
                       pCallbackData->pMessage);
  } else {
    out << std::format("[{}: {}]\n{}\n", ms, mt, pCallbackData->pMessage);
  }
  std::flush(out);

  return VK_FALSE;
}
}

VulkanContext::~VulkanContext()
{
  destroy(*dummy_texture);

  staging_allocator.reset();
  immediate_commands.reset();

  while (!delete_queue.empty()) {
    auto back = std::move(delete_queue.back());
    delete_queue.pop_back();
    back(*this);
  }
  while (!pre_frame_queue.empty()) {
    auto back = std::move(pre_frame_queue.back());
    pre_frame_queue.pop_back();
    back(*this);
  }

  assert(textures.size() == 0);

  DeviceAllocator::deinitialise();

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
    return make_error(ContextError{ InvalidWindow, "Window not initialised" });

  vkb::InstanceBuilder instance_builder;
  auto instance_ret = instance_builder.require_api_version(1, 4)
                        .set_debug_callback(vk_debug_callback)
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
    return make_error(ContextError{ InvalidWindow, "blah" });
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
  required_features.sampleRateShading = true;
  required_features.geometryShader = true;

  VkPhysicalDeviceVulkan11Features required_11_features{};
  required_11_features.shaderDrawParameters = true;

  VkPhysicalDeviceVulkan12Features required_12_features{};
  required_12_features.bufferDeviceAddress = true;
  required_12_features.descriptorIndexing = true;
  required_12_features.timelineSemaphore = true;
  required_12_features.hostQueryReset = true;
  required_12_features.runtimeDescriptorArray = true;
  required_12_features.descriptorBindingSampledImageUpdateAfterBind = true;
  required_12_features.descriptorBindingUniformBufferUpdateAfterBind = true;
  required_12_features.descriptorBindingSampledImageUpdateAfterBind = true;
  required_12_features.descriptorBindingStorageImageUpdateAfterBind = true;
  required_12_features.descriptorBindingStorageBufferUpdateAfterBind = true;
  required_12_features.descriptorBindingUniformTexelBufferUpdateAfterBind =
    true;
  required_12_features.descriptorBindingStorageTexelBufferUpdateAfterBind =
    true;
  required_12_features.descriptorBindingUpdateUnusedWhilePending = true;
  required_12_features.descriptorBindingPartiallyBound = true;
  required_12_features.descriptorBindingVariableDescriptorCount = true;
  required_12_features.shaderInputAttachmentArrayDynamicIndexing = true;
  required_12_features.shaderUniformTexelBufferArrayDynamicIndexing = true;
  required_12_features.shaderStorageTexelBufferArrayDynamicIndexing = true;
  required_12_features.shaderUniformBufferArrayNonUniformIndexing = true;
  required_12_features.shaderSampledImageArrayNonUniformIndexing = true;
  required_12_features.shaderStorageBufferArrayNonUniformIndexing = true;
  required_12_features.shaderStorageImageArrayNonUniformIndexing = true;

  required_12_features.shaderUniformTexelBufferArrayNonUniformIndexing = false;
  required_12_features.shaderStorageTexelBufferArrayNonUniformIndexing = false;
  required_12_features.shaderInputAttachmentArrayNonUniformIndexing = false;

  VkPhysicalDeviceVulkan13Features required_13_features{};
  required_13_features.dynamicRendering = true;
  required_13_features.synchronization2 = true;

  auto phys_device_builder =
    phys_device_selector.set_minimum_version(1, 3)
      .set_surface(surface)
      .add_required_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME)
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

  auto gfam = device.get_queue_index(vkb::QueueType::graphics).value();
  auto pfam = device.get_queue_index(vkb::QueueType::present).value();
  VkQueue gq{}, pq{};
  vkGetDeviceQueue(device, gfam, 0, &gq);
  vkGetDeviceQueue(device, pfam, 0, &pq);

  using P = std::unique_ptr<IContext>;
  auto vk_context = new VulkanContext(std::move(instance),
                                      std::move(device),
                                      std::move(disp),
                                      std::move(inst_disp),
                                      surface,
                                      gq,
                                      gfam,
                                      pq,
                                      pfam);

  DeviceAllocator::initialise(*vk_context);
  return std::expected<P, ContextError>{ std::in_place, vk_context };
}

auto
DeviceAllocator::the() -> VmaAllocator&
{
#ifdef _DEBUG
  static std::mutex allocator_mutex{};
  std::unique_lock lock{ allocator_mutex };
  return allocator;
#else
  return allocator;
#endif
}
auto
DeviceAllocator::initialise(IContext& ctx) -> void
{
  VmaAllocatorCreateInfo create_info{};
  create_info.instance = ctx.get_instance();
  create_info.physicalDevice = ctx.get_physical_device();
  create_info.device = ctx.get_device();
  create_info.vulkanApiVersion = VK_API_VERSION_1_4;
  create_info.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT |
                      VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT |
                      VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;

  auto& current = the();
  if (vmaCreateAllocator(&create_info, &current) != VK_SUCCESS) {
    std::abort();
  }
}

auto
query_vulkan_properties(VkPhysicalDevice physical_device, auto& props) -> void
{
  vkGetPhysicalDeviceProperties(physical_device, &props.base);

  props.fourteen.sType =
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_PROPERTIES;
  props.fourteen.pNext = nullptr;

  props.thirteen.sType =
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES;
  props.thirteen.pNext = &props.fourteen;

  props.twelve.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
  props.twelve.pNext = &props.thirteen;

  props.eleven.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
  props.eleven.pNext = &props.twelve;

  VkPhysicalDeviceProperties2 device_props2{};
  device_props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  device_props2.pNext = &props.eleven;

  vkGetPhysicalDeviceProperties2(physical_device, &device_props2);

  props.base = device_props2.properties;
}

VulkanContext::VulkanContext(vkb::Instance&& i,
                             vkb::Device&& d,
                             vkb::DispatchTable&& dt,
                             vkb::InstanceDispatchTable&& idt,
                             VkSurfaceKHR surf,
                             VkQueue gq,
                             std::uint32_t gfam,
                             VkQueue pq,
                             std::uint32_t pfam)
  : instance(std::move(i))
  , device(std::move(d))
  , dispatch_table(std::move(dt))
  , instance_dispatch_table(std::move(idt))
  , surface(surf)
  , graphics_queue(gq)
  , present_queue(pq)
  , graphics_family(gfam)
  , present_family(pfam)
{
  query_vulkan_properties(device.physical_device, vulkan_properties);
}

auto
VulkanContext::create_placeholder_resources() -> void
{
  const uint32_t pixel = 0xFFFFFFFF;
  std::span white_texture_span(&pixel, 1);
  dummy_texture = VulkanTextureND::create(
    *this,
    {
      .format = Format::RGBA_UN8,
      .usage_bits = TextureUsageBits::Sampled | TextureUsageBits::Storage,
      .pixel_data = std::as_bytes(white_texture_span),
      .debug_name = "White Texture (reserved)",
    });
}

auto
VulkanContext::destroy(TextureHandle handle) -> void
{
  SCOPE_EXIT
  {
    textures.erase(handle);
    needs_descriptor_update = true;
  };
  auto* tex = textures.get(handle);
  if (!tex)
    return;
  defer_task([view = tex->image_view](IContext& ctx) {
    vkDestroyImageView(ctx.get_device(), view, nullptr);
  });
  if (tex->storage_image_view) {
    defer_task([view = tex->storage_image_view](IContext& ctx) {
      vkDestroyImageView(ctx.get_device(), view, nullptr);
    });
  }
  /*
  for (size_t i = 0; i != LVK_MAX_MIP_LEVELS; i++) {
    for (size_t j = 0;
         j != LVK_ARRAY_NUM_ELEMENTS(tex->imageViewForFramebuffer_[0]);
         j++) {
      VkImageView v = tex->imageViewForFramebuffer_[i][j];
      if (v) {
        defer_task([device = get_device(), imageView = v]() {
          vkDestroyImageView(device, imageView, nullptr);
        });
      }
    }
  }
    */

  if (!tex->is_owning_image)
    return;
  if (tex->allocation_info.pMappedData)
    vmaUnmapMemory(DeviceAllocator::the(), tex->allocation);
  defer_task(([image = tex->image, allocation = tex->allocation](IContext&) {
    vmaDestroyImage(DeviceAllocator::the(), image, allocation);
  }));
};

auto VulkanContext::destroy(GraphicsPipelineHandle) -> void{

};
auto VulkanContext::destroy(ComputePipelineHandle) -> void{

};
auto
VulkanContext::destroy(BufferHandle handle) -> void
{
  SCOPE_EXIT
  {
    buffers.erase(handle);
  };
  auto* buf = buffers.get(handle);
  if (!buf)
    return;
  defer_task([buffer = buf->buffer, allocation = buf->allocation](IContext&) {
    vmaDestroyBuffer(DeviceAllocator::the(), buffer, allocation);
  });
}

auto
VulkanContext::flush_mapped_memory(BufferHandle handle, OffsetSize os) const
  -> void
{
  auto* buf = get_buffer_pool().get(handle);
  vmaFlushAllocation(
    DeviceAllocator::the(), buf->allocation, os.offset, os.size);
}

auto
VulkanContext::invalidate_mapped_memory(BufferHandle handle,
                                        OffsetSize os) const -> void
{
  auto* buf = get_buffer_pool().get(handle);
  vmaInvalidateAllocation(
    DeviceAllocator::the(), buf->allocation, os.offset, os.size);
}

}