#include "sv/buffer.hpp"
#include "sv/common.hpp"

#include "sv/context.hpp"

namespace sv {

namespace {

static constexpr auto use_staging = true;
auto
storage_type_to_vk_memory_property_flags(StorageType storage)
{
  VkMemoryPropertyFlags memory_flags{ 0 };

  switch (storage) {
    case StorageType::Device:
      memory_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      break;
    case StorageType::HostVisible:
      memory_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
      break;
    case StorageType::Transient:
      memory_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                      VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
      break;
  }
  return memory_flags;
}

auto
create_buffer(IContext& ctx,
              VkDeviceSize buffer_size,
              VkBufferUsageFlags usage_flags,
              VkMemoryPropertyFlags memory_flags,
              const std::string_view) -> BufferHandle
{
  VulkanDeviceBuffer buf = {
    .usage_flags = usage_flags,
    .memory_flags = memory_flags,
  };
  const VkBufferCreateInfo ci = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .size = buffer_size,
    .usage = usage_flags,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = nullptr,
  };

  VmaAllocationCreateInfo vma_allocation_create_info = {};
  if (memory_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
    vma_allocation_create_info.flags =
      VMA_ALLOCATION_CREATE_MAPPED_BIT |
      VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    vma_allocation_create_info.requiredFlags =
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    vma_allocation_create_info.preferredFlags =
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
  }
  if (memory_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
    vkCreateBuffer(ctx.get_device(), &ci, nullptr, &buf.buffer);
    VkMemoryRequirements requirements = {};
    vkGetBufferMemoryRequirements(ctx.get_device(), buf.buffer, &requirements);
    vkDestroyBuffer(ctx.get_device(), buf.buffer, nullptr);
    buf.buffer = VK_NULL_HANDLE;
    if (requirements.memoryTypeBits & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
      vma_allocation_create_info.requiredFlags |=
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
      buf.is_coherent_memory = true;
    }
  }
  vma_allocation_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  vmaCreateBuffer(DeviceAllocator::the(),
                  &ci,
                  &vma_allocation_create_info,
                  &buf.buffer,
                  &buf.allocation,
                  &buf.allocation_info);

  // setDebugObjectName(
  //   vkDevice_, VK_OBJECT_TYPE_BUFFER, (uint64_t)buf.vkBuffer_, debugName);
  if (usage_flags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
    const VkBufferDeviceAddressInfo ai = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .pNext = nullptr,
      .buffer = buf.buffer,
    };
    buf.device_address = vkGetBufferDeviceAddress(ctx.get_device(), &ai);
  }
  return ctx.get_buffer_pool().insert(std::move(buf));
}

}

auto
VulkanDeviceBuffer::create(IContext& ctx, const BufferDescription& w)
  -> Holder<BufferHandle>
{
  BufferDescription description{ w };
  if constexpr (!use_staging && description.storage == StorageType::Device) {
    description.storage = StorageType::HostVisible;
  }

  VkBufferUsageFlags usage_flags =
    description.storage == StorageType::Device
      ? VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
      : 0;

  if ((description.usage & BufferUsageBits::Index) != BufferUsageBits{ 0 })
    usage_flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  if ((description.usage & BufferUsageBits::Vertex) != BufferUsageBits{ 0 })
    usage_flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  if ((description.usage & BufferUsageBits::Uniform) != BufferUsageBits{ 0 })
    usage_flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  if ((description.usage & BufferUsageBits::Storage) != BufferUsageBits{ 0 })
    usage_flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  if ((description.usage & BufferUsageBits::Indirect) != BufferUsageBits{ 0 })
    usage_flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

  const VkMemoryPropertyFlags memory_flags =
    storage_type_to_vk_memory_property_flags(description.storage);
  BufferHandle handle = create_buffer(
    ctx, description.size, usage_flags, memory_flags, description.debug_name);

  if (!description.data.empty()) {
    auto* buffer = ctx.get_buffer_pool().get(handle);
    buffer->upload(description.data, 0);
    ctx.flush_mapped_memory(handle,
                            {
                              .offset = 0,
                              .size = description.data.size_bytes(),
                            });
  }
  return Holder{ &ctx, handle };
}

auto
VulkanDeviceBuffer::upload(const std::span<const std::byte> data,
                           std::uint64_t offset) -> void
{
  std::memcpy(
    allocation_info.pMappedData, data.data() + offset, data.size_bytes());
  vmaFlushAllocation(
    DeviceAllocator::the(), allocation, offset, data.size_bytes());
}

}