#include "sv/staging_allocator.hpp"

#include "sv/buffer.hpp"
#include "sv/context.hpp"
#include "sv/scope_exit.hpp"
#include "sv/texture.hpp"

#include <format>

namespace sv {

namespace {
auto
vk_format_to_format(const VkFormat format) -> Format
{
  switch (format) {
    case VK_FORMAT_UNDEFINED:
      return Format::Invalid;
    case VK_FORMAT_R8_UINT:
      return Format::R_UI8;
    case VK_FORMAT_R8_UNORM:
      return Format::R_UN8;
    case VK_FORMAT_R16_UINT:
      return Format::R_UI16;
    case VK_FORMAT_R32_UINT:
      return Format::R_UI32;
    case VK_FORMAT_R16_UNORM:
      return Format::R_UN16;
    case VK_FORMAT_R16_SFLOAT:
      return Format::R_F16;
    case VK_FORMAT_R32_SFLOAT:
      return Format::R_F32;

    case VK_FORMAT_R8G8_UNORM:
      return Format::RG_UN8;
    case VK_FORMAT_R16G16_UINT:
      return Format::RG_UI16;
    case VK_FORMAT_R32G32_UINT:
      return Format::RG_UI32;
    case VK_FORMAT_R16G16_UNORM:
      return Format::RG_UN16;
    case VK_FORMAT_R16G16_SFLOAT:
      return Format::RG_F16;
    case VK_FORMAT_R32G32_SFLOAT:
      return Format::RG_F32;

    case VK_FORMAT_R8G8B8A8_UNORM:
      return Format::RGBA_UN8;
    case VK_FORMAT_R32G32B32A32_UINT:
      return Format::RGBA_UI32;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
      return Format::RGBA_F16;
    case VK_FORMAT_R16G16B16A16_UINT:
      return Format::RGBA_UI16;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
      return Format::RGBA_F32;
    case VK_FORMAT_R8G8B8A8_SRGB:
      return Format::RGBA_SRGB8;

    case VK_FORMAT_B8G8R8A8_UNORM:
      return Format::BGRA_UN8;
    case VK_FORMAT_B8G8R8A8_SRGB:
      return Format::BGRA_SRGB8;

    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
      return Format::A2B10G10R10_UN;
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
      return Format::A2R10G10B10_UN;

    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
      return Format::ETC2_RGB8;
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
      return Format::ETC2_SRGB8;
    case VK_FORMAT_BC7_UNORM_BLOCK:
      return Format::BC7_RGBA;

    case VK_FORMAT_D16_UNORM:
      return Format::Z_UN16;
    case VK_FORMAT_X8_D24_UNORM_PACK32:
      return Format::Z_UN24;
    case VK_FORMAT_D32_SFLOAT:
      return Format::Z_F32;
    case VK_FORMAT_D24_UNORM_S8_UINT:
      return Format::Z_UN24_S_UI8;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      return Format::Z_F32_S_UI8;

    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
      return Format::YUV_NV12;
    case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
      return Format::YUV_420p;
    default:
      return Format::Invalid;
  }
}
}

static constexpr VkDeviceSize max_staging_buffer_size =
  256ULL * 1024ULL * 1024ULL; // 256MB

StagingAllocator::StagingAllocator(IContext& ctx)
  : context(dynamic_cast<VulkanContext&>(ctx))
{

  const auto max_memory_allocation_size =
    context.vulkan_properties.eleven.maxMemoryAllocationSize;

  // clamped to the max limits
  max_buffer_size = static_cast<std::uint32_t>(
    std::min(max_memory_allocation_size, max_staging_buffer_size));
  min_buffer_size =
    static_cast<std::uint32_t>(std::min(min_buffer_size, max_buffer_size));
}

void
StagingAllocator::upload(VulkanDeviceBuffer& buffer,
                         std::size_t dstOffset,
                         std::size_t size,
                         const void* data)
{
  if (buffer.is_mapped()) {
    buffer.upload(std::span(static_cast<const std::byte*>(data), size),
                  dstOffset);

    return;
  }

  auto* stg = context.get_buffer_pool().get(*staging_buffer);

  assert(nullptr != stg);

  while (size) {
    // get next staging buffer free offset
    auto desc = get_next_free_offset(static_cast<uint32_t>(size));
    const auto chunkSize = std::min(static_cast<uint64_t>(size), desc.size);

    // copy data into staging buffer
    stg->upload(std::span(static_cast<const std::byte*>(data), chunkSize),
                desc.offset);

    // do the transfer
    const VkBufferCopy copy = {
      .srcOffset = desc.offset,
      .dstOffset = dstOffset,
      .size = chunkSize,
    };

    const auto& wrapper = context.immediate_commands->acquire();
    vkCmdCopyBuffer(
      wrapper.command_buffer, stg->get_buffer(), buffer.get_buffer(), 1, &copy);
    VkBufferMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      .pNext = nullptr,

      .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .dstAccessMask = 0,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = buffer.get_buffer(),
      .offset = dstOffset,
      .size = chunkSize,
    };
    VkPipelineStageFlags dstMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    if (buffer.usage_flags & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) {
      dstMask |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
      barrier.dstAccessMask |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    }
    if (buffer.usage_flags & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) {
      dstMask |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
      barrier.dstAccessMask |= VK_ACCESS_INDEX_READ_BIT;
    }
    if (buffer.usage_flags & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) {
      dstMask |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
      barrier.dstAccessMask |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    }
    if (buffer.usage_flags &
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR) {
      dstMask |= VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
      barrier.dstAccessMask |= VK_ACCESS_MEMORY_READ_BIT;
    }
    vkCmdPipelineBarrier(wrapper.command_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         dstMask,
                         VkDependencyFlags{},
                         0,
                         nullptr,
                         1,
                         &barrier,
                         0,
                         nullptr);
    desc.handle = context.immediate_commands->submit(wrapper);
    regions.push_back(desc);

    size -= chunkSize;
    data = std::bit_cast<std::uint8_t*>(data) + chunkSize;
    dstOffset += chunkSize;
  }
}

struct StageAccess
{
  VkPipelineStageFlags2 stage;
  VkAccessFlags2 access;
};

void
imageMemoryBarrier2(VkCommandBuffer buffer,
                    VkImage image,
                    StageAccess src,
                    StageAccess dst,
                    VkImageLayout oldImageLayout,
                    VkImageLayout newImageLayout,
                    VkImageSubresourceRange subresourceRange)
{
  const VkImageMemoryBarrier2 barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    .pNext = nullptr,
    .srcStageMask = src.stage,
    .srcAccessMask = src.access,
    .dstStageMask = dst.stage,
    .dstAccessMask = dst.access,
    .oldLayout = oldImageLayout,
    .newLayout = newImageLayout,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = image,
    .subresourceRange = subresourceRange,
  };

  const VkDependencyInfo dependency_info = {
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .pNext = nullptr,
    .dependencyFlags = 0,
    .memoryBarrierCount = 0,
    .pMemoryBarriers = nullptr,
    .bufferMemoryBarrierCount = 0,
    .pBufferMemoryBarriers = nullptr,
    .imageMemoryBarrierCount = 1,
    .pImageMemoryBarriers = &barrier,
  };

  vkCmdPipelineBarrier2(buffer, &dependency_info);
}

struct TextureFormatProperties
{
  Format format{ Format::Invalid };
  std::uint8_t bytes_per_block{ 1 };
  std::uint8_t block_width{ 1 };
  std::uint8_t block_height{ 1 };
  std::uint8_t min_blocks_x{ 1 };
  std::uint8_t min_blocks_y{ 1 };
  bool depth{ false };
  bool stencil{ false };
  bool compressed{ false };
  std::uint8_t num_planes{ 1 };
};

constexpr auto properties = std::to_array<TextureFormatProperties>({
  { .format = Format::Invalid },
  { .format = Format::R_UN8, .bytes_per_block = 1 },
  { .format = Format::R_UI16, .bytes_per_block = 2 },
  { .format = Format::R_UI32, .bytes_per_block = 4 },
  { .format = Format::R_UN16, .bytes_per_block = 2 },
  { .format = Format::R_F16, .bytes_per_block = 2 },
  { .format = Format::R_F32, .bytes_per_block = 4 },
  { .format = Format::RG_UN8, .bytes_per_block = 2 },
  { .format = Format::RG_UI16, .bytes_per_block = 4 },
  { .format = Format::RG_UI32, .bytes_per_block = 8 },
  { .format = Format::RG_UN16, .bytes_per_block = 4 },
  { .format = Format::RG_F16, .bytes_per_block = 4 },
  { .format = Format::RG_F32, .bytes_per_block = 8 },
  { .format = Format::RGBA_UN8, .bytes_per_block = 4 },
  { .format = Format::RGBA_UI16, .bytes_per_block = 8 },
  { .format = Format::RGBA_UI32, .bytes_per_block = 16 },
  { .format = Format::RGBA_F16, .bytes_per_block = 8 },
  { .format = Format::RGBA_F32, .bytes_per_block = 16 },
  { .format = Format::RGBA_SRGB8, .bytes_per_block = 4 },
  { .format = Format::BGRA_UN8, .bytes_per_block = 4 },
  { .format = Format::BGRA_SRGB8, .bytes_per_block = 4 },
  { .format = Format::A2B10G10R10_UN, .bytes_per_block = 4 },
  { .format = Format::A2R10G10B10_UN, .bytes_per_block = 4 },
  { .format = Format::ETC2_RGB8,
    .bytes_per_block = 8,
    .block_width = 4,
    .block_height = 4,
    .compressed = true },
  { .format = Format::ETC2_SRGB8,
    .bytes_per_block = 8,
    .block_width = 4,
    .block_height = 4,
    .compressed = true },
  { .format = Format::BC7_RGBA,
    .bytes_per_block = 16,
    .block_width = 4,
    .block_height = 4,
    .compressed = true },
  { .format = Format::Z_UN16, .bytes_per_block = 2, .depth = true },
  { .format = Format::Z_UN24, .bytes_per_block = 3, .depth = true },
  { .format = Format::Z_F32, .bytes_per_block = 4, .depth = true },
  { .format = Format::Z_UN24_S_UI8,
    .bytes_per_block = 4,
    .depth = true,
    .stencil = true },
  { .format = Format::Z_F32_S_UI8,
    .bytes_per_block = 5,
    .depth = true,
    .stencil = true },
  { .format = Format::YUV_NV12,
    .bytes_per_block = 24,
    .block_width = 4,
    .block_height = 4,
    .compressed = true,
    .num_planes = 2 },
  { .format = Format::YUV_420p,
    .bytes_per_block = 24,
    .block_width = 4,
    .block_height = 4,
    .compressed = true,
    .num_planes = 3 },
});

auto
get_texture_bytes_per_layer(const std::uint32_t width,
                            const std::uint32_t height,
                            Format format,
                            const std::uint32_t level) -> std::uint32_t
{
  const uint32_t level_width = std::max(width >> level, 1u);
  const uint32_t level_height = std::max(height >> level, 1u);

  const auto maybe_props = std::ranges::find_if(
    properties,
    [format](const TextureFormatProperties& p) { return p.format == format; });

  if (maybe_props == properties.end() ||
      maybe_props->format == Format::Invalid) {
    return 0;
  }

  const auto props = *maybe_props;
  if (!props.compressed) {
    return props.bytes_per_block * level_width * level_height;
  }

  const uint32_t widthInBlocks =
    (level_width + props.block_width - 1) / props.block_width;
  const uint32_t heightInBlocks =
    (level_height + props.block_height - 1) / props.block_height;
  return widthInBlocks * heightInBlocks * props.bytes_per_block;
}

auto
get_num_image_planes(Format format) -> std::uint32_t
{
  const auto maybe_props = std::ranges::find_if(
    properties,
    [format](const TextureFormatProperties& p) { return p.format == format; });

  if (maybe_props == properties.end() ||
      maybe_props->format == Format::Invalid) {
    return 0;
  }

  return maybe_props->num_planes;
}

VkExtent2D
get_image_plane_extent(VkExtent2D plane0, Format format, uint32_t plane)
{
  switch (format) {
    case Format::YUV_NV12:
      return VkExtent2D{
        .width = plane0.width >> plane,
        .height = plane0.height >> plane,
      };
    case Format::YUV_420p:
      return VkExtent2D{
        .width = plane0.width >> (plane ? 1 : 0),
        .height = plane0.height >> (plane ? 1 : 0),
      };
    default:
      return plane0;
  }
}

auto
get_texture_bytes_per_plane(const std::uint32_t width,
                            const std::uint32_t height,
                            Format format,
                            std::uint32_t plane) -> std::uint32_t
{
  const TextureFormatProperties props = *std::ranges::find_if(
    properties,
    [format](const TextureFormatProperties& p) { return p.format == format; });

  assert(plane < props.num_planes);

  switch (format) {
    case Format::YUV_NV12:
      return width * height / (plane + 1);
    case Format::YUV_420p:
      return width * height / (plane ? 4 : 1);
    default:;
  }

  return get_texture_bytes_per_layer(width, height, format, 0);
}

void
StagingAllocator::generate_mipmaps(VulkanTextureND& texture,
                                   uint32_t texWidth,
                                   uint32_t texHeight,
                                   uint32_t mip_levels,
                                   uint32_t layers)
{
  const auto& wrapper = context.immediate_commands->acquire();
  VkImage image = texture.image;

  int32_t mipWidth = static_cast<int32_t>(texWidth);
  int32_t mipHeight = static_cast<int32_t>(texHeight);

  // --- Step 0: Transition mip 0 to TRANSFER_DST_OPTIMAL ---
  {
    VkImageMemoryBarrier2 barrier
    {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, .pNext = nullptr,
      .srcStageMask = VK_PIPELINE_STAGE_2_NONE, .srcAccessMask = 0,
      .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = layers,
      },
    };

    VkDependencyInfo dependency_info{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .pNext = nullptr,
      .dependencyFlags = 0,
      .memoryBarrierCount = 0,
      .pMemoryBarriers = nullptr,
      .bufferMemoryBarrierCount = 0,
      .pBufferMemoryBarriers = nullptr,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &barrier,
    };

    vkCmdPipelineBarrier2(wrapper.command_buffer, &dependency_info);
  }

  // --- Generate each mip ---
  for (std::uint32_t i = 1; i < mip_levels; i++) {
    // 1. Transition previous mip (i-1) to TRANSFER_SRC_OPTIMAL
    {
      VkImageMemoryBarrier2 barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
      barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
      barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      barrier.image = image;
      barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      barrier.subresourceRange.baseArrayLayer = 0;
      barrier.subresourceRange.layerCount = layers;
      barrier.subresourceRange.baseMipLevel = i - 1;
      barrier.subresourceRange.levelCount = 1;

      VkDependencyInfo dependency_info{};
      dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
      dependency_info.imageMemoryBarrierCount = 1;
      dependency_info.pImageMemoryBarriers = &barrier;

      vkCmdPipelineBarrier2(wrapper.command_buffer, &dependency_info);
    }

    // 2. Transition current mip (i) to TRANSFER_DST_OPTIMAL
    {
      VkImageMemoryBarrier2 barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
      barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
      barrier.srcAccessMask = 0;
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
      barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barrier.image = image;
      barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      barrier.subresourceRange.baseArrayLayer = 0;
      barrier.subresourceRange.layerCount = layers;
      barrier.subresourceRange.baseMipLevel = i;
      barrier.subresourceRange.levelCount = 1;

      VkDependencyInfo dependency_info{};
      dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
      dependency_info.imageMemoryBarrierCount = 1;
      dependency_info.pImageMemoryBarriers = &barrier;

      vkCmdPipelineBarrier2(wrapper.command_buffer, &dependency_info);
    }

    // 3. Blit previous mip (i-1) -> current mip (i)
    VkImageBlit blit{};
    blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, layers };
    blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
    blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i, 0, layers };
    blit.dstOffsets[1] = { std::max(1, mipWidth / 2),
                           std::max(1, mipHeight / 2),
                           1 };

    vkCmdBlitImage(wrapper.command_buffer,
                   image,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1,
                   &blit,
                   VK_FILTER_LINEAR);

    // 4. Transition previous mip to SHADER_READ_ONLY_OPTIMAL
    {
      VkImageMemoryBarrier2 barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
      barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
      barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
      barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      barrier.image = image;
      barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      barrier.subresourceRange.baseArrayLayer = 0;
      barrier.subresourceRange.layerCount = layers;
      barrier.subresourceRange.baseMipLevel = i - 1;
      barrier.subresourceRange.levelCount = 1;

      VkDependencyInfo dependency_info{};
      dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
      dependency_info.imageMemoryBarrierCount = 1;
      dependency_info.pImageMemoryBarriers = &barrier;

      vkCmdPipelineBarrier2(wrapper.command_buffer, &dependency_info);
    }

    // Reduce mip width/height for next iteration
    mipWidth = std::max(1, mipWidth / 2);
    mipHeight = std::max(1, mipHeight / 2);
  }

  // --- Transition last mip to SHADER_READ_ONLY_OPTIMAL ---
  {
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layers;
    barrier.subresourceRange.baseMipLevel = mip_levels - 1;
    barrier.subresourceRange.levelCount = 1;

    VkDependencyInfo dependency_info{};
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency_info.imageMemoryBarrierCount = 1;
    dependency_info.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(wrapper.command_buffer, &dependency_info);
  }

  // Submit command buffer and wait
  context.immediate_commands->wait(context.immediate_commands->submit(wrapper));
}

auto
StagingAllocator::upload_blob_with_regions(
  VulkanTextureND& image,
  std::span<const VkBufferImageCopy> regions_in,
  const void* blob,
  uint32_t blob_size) -> void
{
  ensure_size(blob_size);
  auto desc = get_next_free_offset(blob_size);
  if (desc.size < blob_size) {
    wait_and_reset();
    desc = get_next_free_offset(blob_size);
  }

  auto* staging = context.get_buffer_pool().get(*staging_buffer);
  staging->upload(std::span(static_cast<const std::byte*>(blob), blob_size),
                  desc.offset);

  std::vector<VkBufferImageCopy> copy_regions;
  copy_regions.reserve(regions_in.size());
  for (const auto& r : regions_in) {
    auto rr = r;
    rr.bufferOffset += desc.offset;
    copy_regions.push_back(rr);
  }

  const auto& w = context.immediate_commands->acquire();

  VkImageSubresourceRange range{};
  range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  range.baseMipLevel = 0;
  range.levelCount = image.level_count;
  range.baseArrayLayer = 0;
  range.layerCount = image.layer_count;

  imageMemoryBarrier2(
    w.command_buffer,
    image.image,
    { VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE },
    { VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT },
    image.image_layout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_UNDEFINED
                                                    : image.image_layout,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    range);

  vkCmdCopyBufferToImage(w.command_buffer,
                         staging->get_buffer(),
                         image.image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         static_cast<uint32_t>(copy_regions.size()),
                         copy_regions.data());

  imageMemoryBarrier2(
    w.command_buffer,
    image.image,
    { VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT },
    { VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT },
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    range);

  // image.set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  desc.handle = context.immediate_commands->submit(w);
  this->regions.push_back(desc);
}

void
StagingAllocator::upload(TextureHandle handle,
    const VkRect2D& image_region,
    std::uint32_t base_mip_level,
    std::uint32_t mip_level_count,
    std::uint32_t layer,
    std::uint32_t num_layers,
    VkFormat format,
    const void* data,
    std::uint32_t buffer_row_length)
{
  auto* img = context.get_texture_pool().get(handle);
  if (!img)
    return;

upload(*img,
        image_region,
        base_mip_level,
        mip_level_count,
        layer,
        num_layers,
        format,
        data,
        buffer_row_length);
}

void
StagingAllocator::upload(VulkanTextureND& image,
                         const VkRect2D& image_region,
                         std::uint32_t base_mip_level,
                         std::uint32_t mip_level_count,
                         std::uint32_t layer,
                         std::uint32_t num_layers,
                         VkFormat format,
                         const void* data,
                         std::uint32_t buffer_row_length)
{
  // assert(mip_level_count <= LVK_MAX_MIP_LEVELS);

  const Format texFormat = vk_format_to_format(format);

  // divide the width and height by 2 until we get to the size of level
  // 'base_mip_level'
  const std::uint32_t width = image.extent.width >> base_mip_level;
  const std::uint32_t height = image.extent.height >> base_mip_level;
  const bool covers_full_image =
    !image_region.offset.x && !image_region.offset.y &&
    image_region.extent.width == width && image_region.extent.height == height;

  // LVK_ASSERT(covers_full_image || image.vkImageLayout_ !=
  // VK_IMAGE_LAYOUT_UNDEFINED);

  if (mip_level_count > 1 || num_layers > 1) {
    assert(!buffer_row_length);
    assert(covers_full_image);
  }

  // find the storage size for all mip-levels being uploaded
  std::uint32_t layerStorageSize = 0;
  for (std::uint32_t i = 0; i < mip_level_count; ++i) {
    const std::uint32_t mipSize = get_texture_bytes_per_layer(
      buffer_row_length ? buffer_row_length : image_region.extent.width,
      image_region.extent.height,
      texFormat,
      i);
    layerStorageSize += mipSize;
  }

  const std::uint32_t storage_size = layerStorageSize * num_layers;

  ensure_size(storage_size);

  assert(storage_size <= staging_buffer_size);

  auto desc = get_next_free_offset(storage_size);
  // No support for copying image in multiple smaller chunk sizes. If we get
  // smaller buffer size than storage_size, we will wait for GPU idle and get
  // bigger chunk.
  if (desc.size < storage_size) {
    wait_and_reset();
    desc = get_next_free_offset(storage_size);
  }
  assert(desc.size >= storage_size);

  const auto& wrapper = context.immediate_commands->acquire();

  auto* stb = context.get_buffer_pool().get(*staging_buffer);

  stb->upload(std::span(static_cast<const std::byte*>(data), storage_size),
              desc.offset);

  std::uint32_t offset = 0;

  const std::uint32_t numPlanes =
    get_num_image_planes(vk_format_to_format(image.format));

  // if (numPlanes > 1) {
  //   LVK_ASSERT(layer == 0 && base_mip_level == 0);
  //   LVK_ASSERT(num_layers == 1 && mip_level_count == 1);
  //   LVK_ASSERT(image_region.offset.x == 0 && image_region.offset.y == 0);
  //   LVK_ASSERT(image.vkType_ == VK_IMAGE_TYPE_2D);
  //   LVK_ASSERT(image.vkExtent_.width == image_region.extent.width &&
  //   image.vkExtent_.height == image_region.extent.height);
  // }

  VkImageAspectFlags imageAspect = VK_IMAGE_ASPECT_COLOR_BIT;

  if (numPlanes == 2) {
    imageAspect = VK_IMAGE_ASPECT_PLANE_0_BIT | VK_IMAGE_ASPECT_PLANE_1_BIT;
  }
  if (numPlanes == 3) {
    imageAspect = VK_IMAGE_ASPECT_PLANE_0_BIT | VK_IMAGE_ASPECT_PLANE_1_BIT |
                  VK_IMAGE_ASPECT_PLANE_2_BIT;
  }

  // https://registry.khronos.org/KTX/specs/1.0/ktxspec.v1.html
  for (std::uint32_t mipLevel = 0; mipLevel < mip_level_count; ++mipLevel) {
    for (std::uint32_t l = 0; l != num_layers; l++) {
      const std::uint32_t currentMipLevel = base_mip_level + mipLevel;

      // LVK_ASSERT(currentMipLevel < image.numLevels_);
      // LVK_ASSERT(mipLevel < image.numLevels_);

      // 1. Transition initial image layout into TRANSFER_DST_OPTIMAL
      imageMemoryBarrier2(
        wrapper.command_buffer,
        image.image,
        StageAccess{ .stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                     .access = VK_ACCESS_2_NONE },
        StageAccess{ .stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                     .access = VK_ACCESS_2_TRANSFER_WRITE_BIT },
        covers_full_image ? VK_IMAGE_LAYOUT_UNDEFINED : image.image_layout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VkImageSubresourceRange{
          imageAspect,
          currentMipLevel,
          1,
          layer + l,
          1,
        });

      // 2. Copy the pixel data from the staging buffer into the image
      std::uint32_t planeOffset = 0;
      for (std::uint32_t plane = 0; plane != numPlanes; plane++) {
        const VkExtent2D extent = get_image_plane_extent(
          {
            .width = std::max(1u, image_region.extent.width >> mipLevel),
            .height = std::max(1u, image_region.extent.height >> mipLevel),
          },
          vk_format_to_format(format),
          plane);
        const VkRect2D region = {
          .offset = { .x = image_region.offset.x >> mipLevel,
                      .y = image_region.offset.y >> mipLevel },
          .extent = extent,
        };
        const VkBufferImageCopy copy = {
          // the offset for this level is at the start of all mip-levels plus
          // the size of all previous mip-levels being uploaded
          .bufferOffset = desc.offset + offset + planeOffset,
          .bufferRowLength = buffer_row_length,
          .bufferImageHeight = 0,
          .imageSubresource =
            VkImageSubresourceLayers{
              numPlanes > 1 ? VK_IMAGE_ASPECT_PLANE_0_BIT << plane
                            : imageAspect,
              currentMipLevel,
              l + layer,
              1,
            },
          .imageOffset = { .x = region.offset.x, .y = region.offset.y, .z = 0, },
          .imageExtent = { .width = region.extent.width,
                           .height = region.extent.height,
                           .depth = 1u, },
        };
        vkCmdCopyBufferToImage(wrapper.command_buffer,
                               stb->get_buffer(),
                               image.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1,
                               &copy);
        planeOffset += get_texture_bytes_per_plane(image_region.extent.width,
                                                   image_region.extent.height,
                                                   vk_format_to_format(format),
                                                   plane);
      }

      // 3. Transition TRANSFER_DST_OPTIMAL into SHADER_READ_ONLY_OPTIMAL
      imageMemoryBarrier2(
        wrapper.command_buffer,
        image.image,
        StageAccess{ .stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                     .access = VK_ACCESS_2_TRANSFER_WRITE_BIT },
        StageAccess{ .stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                     .access = VK_ACCESS_2_MEMORY_READ_BIT |
                               VK_ACCESS_2_MEMORY_WRITE_BIT },
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VkImageSubresourceRange{
          imageAspect,
          currentMipLevel,
          1,
          l + layer,
          1,
        });

      offset += get_texture_bytes_per_layer(image_region.extent.width,
                                            image_region.extent.height,
                                            texFormat,
                                            currentMipLevel);
    }
  }

  //  image.set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  desc.handle = context.immediate_commands->submit(wrapper);
  regions.push_back(desc);
}

void
StagingAllocator::upload(VulkanTextureND& image,
                         const void* data,
                         std::size_t data_bytes,
                         std::span<const VkBufferImageCopy> copies)
{
  ensure_size(static_cast<std::uint32_t>(data_bytes));

  auto desc = get_next_free_offset(static_cast<std::uint32_t>(data_bytes));
  if (desc.size < data_bytes) {
    wait_and_reset();
    desc = get_next_free_offset(static_cast<std::uint32_t>(data_bytes));
  }

  auto* real_buffer = context.get_buffer_pool().get(*staging_buffer);
  real_buffer->upload(

    std::span(static_cast<const std::byte*>(data), data_bytes), desc.offset);

  const auto& wrapper = context.immediate_commands->acquire();

  VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;

  // Transition all subresources to TRANSFER_DST
  imageMemoryBarrier2(
    wrapper.command_buffer,
    image.image,
    { VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE },
    { VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT },
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    VkImageSubresourceRange{
      aspect, 0, image.level_count, 0, image.layer_count });

  std::vector<VkBufferImageCopy> patched;
  patched.reserve(copies.size());
  for (const auto& c : copies) {
    VkBufferImageCopy pc = c;
    pc.bufferOffset += desc.offset;
    patched.push_back(pc);
  }

  vkCmdCopyBufferToImage(wrapper.command_buffer,
                         real_buffer->get_buffer(),
                         image.image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         static_cast<uint32_t>(patched.size()),
                         patched.data());

  imageMemoryBarrier2(
    wrapper.command_buffer,
    image.image,
    { VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT },
    { VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT },
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VkImageSubresourceRange{
      aspect, 0, image.level_count, 0, image.layer_count });

  // image.set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  desc.handle = context.immediate_commands->submit(wrapper);
  regions.push_back(desc);
}

void
StagingAllocator::ensure_size(std::uint32_t size_needed)
{
  const auto aligned_size = std::max(
    get_aligned_size(size_needed, staging_buffer_alignment), min_buffer_size);

  const auto found_max = aligned_size < max_staging_buffer_size
                           ? aligned_size
                           : max_staging_buffer_size;
  size_needed = static_cast<std::uint32_t>(found_max);

  if (!staging_buffer.empty()) {
    const bool is_enough_size = size_needed <= staging_buffer_size;
    const bool is_max_size = staging_buffer_size == max_staging_buffer_size;

    if (is_enough_size || is_max_size) {
      return;
    }
  }

  wait_and_reset();

  // deallocate the previous staging buffer
  staging_buffer = nullptr;

  // if the combined size of the new staging buffer and the existing one is
  // larger than the limit imposed by some architectures on buffers that are
  // device and host visible, we need to wait for the current buffer to be
  // destroyed before we can allocate a new one
  if ((size_needed + staging_buffer_size) > max_staging_buffer_size) {
    for (auto& c : context.pre_frame_queue) {
      c(context);
    }
    context.pre_frame_queue.clear();
  }

  staging_buffer_size = size_needed;

  auto name = std::format("Staging Buffer {}", staging_buffer_count++);

  staging_buffer = VulkanDeviceBuffer::create(context,
                                              {
                                                .usage = BufferUsageBits::Destination | BufferUsageBits::Source,
                                                .storage = StorageType::Device,
                                                .size = staging_buffer_size,
                                                .debug_name = "Staging buffer",
                                              });

  assert(!staging_buffer.empty());

  regions.clear();
  regions.push_back({ 0, staging_buffer_size, SubmitHandle() });
}

auto
StagingAllocator::get_next_free_offset(uint32_t size)
  -> StagingAllocator::MemoryRegionDescription
{
  const auto requested_aligned_size =
    get_aligned_size(size, staging_buffer_alignment);

  ensure_size(static_cast<std::uint32_t>(requested_aligned_size));

  assert(!regions.empty());

  // if we can't find an available region that is big enough to store
  // requested_aligned_size, return whatever we could find, which will be stored
  // in best_next_iterator
  auto best_next_iterator = regions.begin();

  for (auto it = regions.begin(); it != regions.end(); ++it) {
    if (context.immediate_commands->is_ready(it->handle)) {
      // This region is free, but is it big enough?
      if (it->size >= requested_aligned_size) {
        // It is big enough!
        const auto unused_size = it->size - requested_aligned_size;
        const auto unused_offset = it->offset + requested_aligned_size;

        // Return this region and add the remaining unused size to the regions
        // deque
        SCOPE_EXIT
        {
          regions.erase(it);
          if (unused_size > 0) {
            regions.insert(regions.begin(),
                           { unused_offset, unused_size, SubmitHandle() });
          }
        };

        return { it->offset, requested_aligned_size, SubmitHandle() };
      }
      // cache the largest available region that isn't as big as the one we're
      // looking for
      if (it->size > best_next_iterator->size) {
        best_next_iterator = it;
      }
    }
  }

  // we found a region that is available that is smaller than the requested
  // size. It's the best we can do
  if (best_next_iterator != regions.end() &&
      context.immediate_commands->is_ready(best_next_iterator->handle)) {
    SCOPE_EXIT
    {
      regions.erase(best_next_iterator);
    };

    return { best_next_iterator->offset,
             best_next_iterator->size,
             SubmitHandle() };
  }

  // nothing was available. Let's wait for the entire staging buffer to become
  // free
  wait_and_reset();

  // waitAndReset() adds a region that spans the entire buffer. Since we'll be
  // using part of it, we need to replace it with a used block and an unused
  // portion
  regions.clear();

  // store the unused size in the deque first...
  const auto unused_size = staging_buffer_size > requested_aligned_size
                             ? staging_buffer_size - requested_aligned_size
                             : 0;

  if (unused_size) {
    const auto unused_offset = staging_buffer_size - unused_size;
    regions.insert(regions.begin(),
                   { unused_offset, unused_size, SubmitHandle() });
  }

  // ...and then return the smallest free region that can hold the requested
  // size
  return {
    .offset = 0,
    .size = staging_buffer_size - unused_size,
    .handle = SubmitHandle(),
  };
}

void
StagingAllocator::wait_and_reset()
{
  for (const auto& r : regions) {
    context.immediate_commands->wait(r.handle);
  };

  regions.clear();
  regions.push_back({ 0, staging_buffer_size, SubmitHandle() });
}
}