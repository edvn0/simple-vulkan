#pragma once

#include "sv/common.hpp"
#include "sv/object_handle.hpp"
#include "sv/object_holder.hpp"

#include <vulkan/vulkan.h>

namespace sv {

struct IContext;
class VulkanContext;
struct VulkanTextureND;
struct VulkanDeviceBuffer;

class StagingAllocator final
{
public:
  explicit StagingAllocator(IContext& ctx);
  ~StagingAllocator() = default;

  StagingAllocator(const StagingAllocator&) = delete;
  StagingAllocator& operator=(const StagingAllocator&) = delete;

  void upload(VulkanDeviceBuffer& buffer,
              size_t dstOffset,
              size_t size,
              const void* data);
  void upload(VulkanTextureND& image,
              const VkRect2D& imageRegion,
              std::uint32_t baseMipLevel,
              std::uint32_t numMipLevels,
              std::uint32_t layer,
              std::uint32_t numLayers,
              VkFormat format,
              const void* data,
              std::uint32_t bufferRowLength);
  void upload(TextureHandle image,
              const VkRect2D& imageRegion,
              std::uint32_t baseMipLevel,
              std::uint32_t numMipLevels,
              std::uint32_t layer,
              std::uint32_t numLayers,
              VkFormat format,
              const void* data,
              std::uint32_t bufferRowLength);
  void upload(VulkanTextureND& image,
              const void* data,
              std::size_t data_bytes,
              std::span<const VkBufferImageCopy> copies);
  auto upload_blob_with_regions(VulkanTextureND& image,
                                std::span<const VkBufferImageCopy> regions_in,
                                const void* blob,
                                uint32_t blob_size) -> void;
  auto generate_mipmaps(VulkanTextureND&,
                        std::uint32_t width,
                        std::uint32_t height,
                        std::uint32_t mips,
                        std::uint32_t layer_count) -> void;

private:
  static constexpr auto staging_buffer_alignment = 16ULL;

  struct MemoryRegionDescription
  {
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    SubmitHandle handle = {};
  };

  auto get_next_free_offset(std::uint32_t) -> MemoryRegionDescription;
  auto ensure_size(std::uint32_t) -> void;
  auto wait_and_reset() -> void;

  VulkanContext& context;
  Holder<BufferHandle> staging_buffer;
  VkDeviceSize staging_buffer_size = 0;
  std::uint32_t staging_buffer_count = 0;
  // the staging buffer grows from minBufferSize up to maxBufferSize as needed
  VkDeviceSize max_buffer_size = 0;
  VkDeviceSize min_buffer_size = 4ULL * 2048ULL * 2048ULL;
  std::vector<MemoryRegionDescription> regions{};
};

}
