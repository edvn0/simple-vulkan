#pragma once

#include "sv/common.hpp"
#include "sv/object_handle.hpp"
#include "sv/object_holder.hpp"

#include <cstdint>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace sv {

enum class BufferUsageBits : std::uint8_t
{
  Index = bit(0),
  Vertex = bit(1),
  Uniform = bit(2),
  Storage = bit(3),
  Indirect = bit(4),
  Destination = bit(5),
  Source = bit(6),
};
BIT_FIELD(BufferUsageBits)

struct BufferDescription
{
  std::span<const std::byte> data{};
  BufferUsageBits usage{ 0 };
  StorageType storage{ StorageType::HostVisible };
  std::size_t size{ 0 }; // Will be aligned according to min alignment of the
  // <uniform/storage/etc> arrays of the physical device.
  std::string debug_name;
};

struct VulkanDeviceBuffer
{
  VkDeviceAddress device_address{ 0 };
  VmaAllocation allocation{};
  VmaAllocationInfo allocation_info{};
  VkBuffer buffer{ VK_NULL_HANDLE };
  VkBufferUsageFlags usage_flags{ 0 };
  VkMemoryPropertyFlags memory_flags{ 0 };
  bool is_coherent_memory{ false };

  auto is_mapped() const { return allocation_info.pMappedData != nullptr; }
  auto get_device_address() const { return device_address; }
  auto get_buffer() const -> VkBuffer { return buffer; }
  auto upload(const std::span<const std::byte> data,
              std::uint64_t offset = 0,
              IContext* = nullptr) -> void;

  static auto create(IContext&, const BufferDescription&)
    -> Holder<BufferHandle>;
};

}