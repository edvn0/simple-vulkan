#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <vulkan/vulkan.h>

namespace sv {

struct IContext;

struct DescriptorArrays
{
  VkDescriptorSetLayout layout{ VK_NULL_HANDLE };
  VkDescriptorPool pool{ VK_NULL_HANDLE };
  VkDescriptorSet set{ VK_NULL_HANDLE };
  std::uint32_t sampled_capacity{ 16 };
  std::uint32_t storage_capacity{ 16 };
};

template<typename Ctx>
struct BindlessAccess
{
  static auto device(Ctx&) -> VkDevice = delete;
  static auto descriptors(Ctx&) -> DescriptorArrays& = delete;
  static auto needs_descriptor_update(Ctx&) -> bool& = delete;
  static auto enqueue_destruction(Ctx&,
                                  std::function<void(IContext&)>&&) = delete;
  static auto process_pre_frame_task(Ctx&) = delete;
  static auto defer_task(Ctx&, std::function<void(IContext&)>&&) = delete;
  static auto wait_for_latest(Ctx&) = delete;
};

}