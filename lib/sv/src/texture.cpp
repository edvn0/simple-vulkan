#include "sv/texture.hpp"

#include "sv/context.hpp"
#include "vulkan/vulkan_core.h"
#include <format>

namespace sv {
namespace {
auto
format_to_vk_format(const Format format) -> VkFormat
{
  switch (format) {
    case Format::Invalid:
      return VK_FORMAT_UNDEFINED;

    case Format::R_UI8:
      return VK_FORMAT_R8_UINT;
    case Format::R_UN8:
      return VK_FORMAT_R8_UNORM;
    case Format::R_UI16:
      return VK_FORMAT_R16_UINT;
    case Format::R_UI32:
      return VK_FORMAT_R32_UINT;
    case Format::R_UN16:
      return VK_FORMAT_R16_UNORM;
    case Format::R_F16:
      return VK_FORMAT_R16_SFLOAT;
    case Format::R_F32:
      return VK_FORMAT_R32_SFLOAT;

    case Format::RG_UN8:
      return VK_FORMAT_R8G8_UNORM;
    case Format::RG_UI16:
      return VK_FORMAT_R16G16_UINT;
    case Format::RG_UI32:
      return VK_FORMAT_R32G32_UINT;
    case Format::RG_UN16:
      return VK_FORMAT_R16G16_UNORM;
    case Format::RG_F16:
      return VK_FORMAT_R16G16_SFLOAT;
    case Format::RG_F32:
      return VK_FORMAT_R32G32_SFLOAT;

    case Format::RGBA_UN8:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case Format::RGBA_UI32:
      return VK_FORMAT_R32G32B32A32_UINT;
    case Format::RGBA_UI16:
      return VK_FORMAT_R16G16B16A16_UINT;
    case Format::RGBA_F16:
      return VK_FORMAT_R16G16B16A16_SFLOAT;
    case Format::RGBA_F32:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    case Format::RGBA_SRGB8:
      return VK_FORMAT_R8G8B8A8_SRGB;

    case Format::BGRA_UN8:
      return VK_FORMAT_B8G8R8A8_UNORM;
    case Format::BGRA_SRGB8:
      return VK_FORMAT_B8G8R8A8_SRGB;

    case Format::A2B10G10R10_UN:
      return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case Format::A2R10G10B10_UN:
      return VK_FORMAT_A2R10G10B10_UNORM_PACK32;

    case Format::ETC2_RGB8:
      return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
    case Format::ETC2_SRGB8:
      return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
    case Format::BC7_RGBA:
      return VK_FORMAT_BC7_UNORM_BLOCK;

    case Format::Z_UN16:
      return VK_FORMAT_D16_UNORM;
    case Format::Z_UN24:
      return VK_FORMAT_X8_D24_UNORM_PACK32;
    case Format::Z_F32:
      return VK_FORMAT_D32_SFLOAT;
    case Format::Z_UN24_S_UI8:
      return VK_FORMAT_D24_UNORM_S8_UINT;
    case Format::Z_F32_S_UI8:
      return VK_FORMAT_D32_SFLOAT_S8_UINT;

    case Format::YUV_NV12:
      return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
    case Format::YUV_420p:
      return VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;
  }
  return VK_FORMAT_UNDEFINED;
}

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
format_is_depth(const VkFormat format) -> bool
{
  switch (format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      return true;
    default:
      return false;
  }
}

auto
format_is_stencil(const VkFormat format) -> bool
{
  switch (format) {
    case VK_FORMAT_S8_UINT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      return true;
    default:
      return false;
  }
}

auto
is_depth_or_stencil_format(const VkFormat format) -> bool
{
  return format_is_depth(format) || format_is_stencil(format);
}

auto
is_depth_or_stencil_format(const Format format) -> bool
{
  return is_depth_or_stencil_format(format_to_vk_format(format));
}
}

auto
VulkanTextureND::create(IContext& ctx, const TextureDescription& desc)
  -> Holder<TextureHandle>
{
  assert(!desc.debug_name.empty());

  VkImageUsageFlags usage_flags =
    (desc.storage & StorageType::Device) != StorageType{ 0 }
      ? VK_IMAGE_USAGE_TRANSFER_DST_BIT
      : 0;
  if ((desc.usage_bits & TextureUsageBits::Sampled) != TextureUsageBits{ 0 }) {
    usage_flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }
  if ((desc.usage_bits & TextureUsageBits::Storage) != TextureUsageBits{ 0 }) {
    usage_flags |= VK_IMAGE_USAGE_STORAGE_BIT;
  }
  if ((desc.usage_bits & TextureUsageBits::Attachment) !=
      TextureUsageBits{ 0 }) {
    usage_flags |= is_depth_or_stencil_format(desc.format)
                     ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                     : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (desc.storage == StorageType::Transient) {
      usage_flags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    }
  }
    

  if (desc.storage != StorageType::Transient) {
    usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  };
  const VkMemoryPropertyFlags memory_flags =
    storage_type_to_vk_memory_property_flags(desc.storage);

  const auto image_debug_name = std::format("Image{}", desc.debug_name);
  const auto view_debug_name = std::format("ImageView{}", desc.debug_name);

  VkImageCreateFlags create_flags = 0;
  VkImageViewType image_view_type{ VK_IMAGE_VIEW_TYPE_2D };
  VkImageType image_type{ VK_IMAGE_TYPE_2D };
  VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
  std::uint32_t layer_count = desc.layer_count;
  switch (desc.type) {
    case TextureType::Two:
      image_view_type =
        layer_count > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
      image_type = VK_IMAGE_TYPE_2D;
      // sample_count = lvk::getVulkanSampleCountFlags(desc.numSamples,
      //                                            getFramebufferMSAABitMask());
      break;
    case TextureType::Three:
      image_view_type = VK_IMAGE_VIEW_TYPE_3D;
      image_type = VK_IMAGE_TYPE_3D;
      break;
    case TextureType::Cube:
      image_view_type = layer_count > 1 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
                                        : VK_IMAGE_VIEW_TYPE_CUBE;
      image_type = VK_IMAGE_TYPE_2D;
      create_flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
      layer_count *= 6;
      break;
  }

  const VkExtent3D extent{ desc.dimensions.width,
                           desc.dimensions.height,
                           desc.dimensions.depth };
  const auto level_count = desc.mip_count;

  const auto vulkan_format = format_to_vk_format(desc.format);

  VulkanTextureND image{
    .usage_flags = usage_flags,
    .extent = extent,
    .type = image_type,
    .format = vulkan_format,
    .samples = sample_count,
    .level_count = level_count,
    .layer_count = layer_count,
    .is_depth_format = format_is_depth(vulkan_format),
    .is_stencil_format = format_is_stencil(vulkan_format),
  };

  const VkImageCreateInfo ci = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext = nullptr,
    .flags = create_flags,
    .imageType = image_type,
    .format = vulkan_format,
    .extent = extent,
    .mipLevels = level_count,
    .arrayLayers = layer_count,
    .samples = sample_count,
    .tiling = static_cast<VkImageTiling>(desc.tiling),
    .usage = usage_flags,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = nullptr,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = memory_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                       ? VMA_MEMORY_USAGE_AUTO
                       : VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  alloc_info.flags = memory_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                       ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                       : VmaAllocationCreateFlags{ 0 };
  alloc_info.priority = 1.0F;
  vmaCreateImage(DeviceAllocator::the(),
                 &ci,
                 &alloc_info,
                 &image.image,
                 &image.allocation,
                 &image.allocation_info);

  // set_debug_object_name(
  //   vkDevice_, VK_OBJECT_TYPE_IMAGE, (uint64_t)image.vkImage_,
  //   debugNameImage);
  vkGetPhysicalDeviceFormatProperties(
    ctx.get_physical_device(), image.format, &image.format_properties);

  VkImageAspectFlags aspect = 0;
  if (image.is_depth_format || image.is_stencil_format) {
    if (image.is_depth_format) {
      aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
    } else if (image.is_stencil_format) {
      aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
  } else {
    aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  }
  const VkComponentMapping mapping = {
    .r = static_cast<VkComponentSwizzle>(desc.swizzle.r),
    .g = static_cast<VkComponentSwizzle>(desc.swizzle.g),
    .b = static_cast<VkComponentSwizzle>(desc.swizzle.b),
    .a = static_cast<VkComponentSwizzle>(desc.swizzle.a),
  };

  image.image_view = image.create_image_view(ctx,
                                             vulkan_format,
                                             aspect,
                                             image_debug_name,
                                             VK_REMAINING_MIP_LEVELS,
                                             layer_count,
                                             image_view_type,
                                             mapping);
  if (image.usage_flags & VK_IMAGE_USAGE_STORAGE_BIT) {
    if (!desc.swizzle.identity()) {
      image.storage_image_view =
        image.create_image_view(ctx,
                                vulkan_format,
                                aspect,
                                image_debug_name,
                                VK_REMAINING_MIP_LEVELS,
                                layer_count,
                                image_view_type,
                                mapping);
    }
  }

  TextureHandle handle = ctx.get_texture_pool().insert(std::move(image));
  ctx.update_resources();
  if (!desc.pixel_data.empty()) {
    ctx.get_staging_allocator().upload(handle,
      VkRect2D{ .offset = { 0, 0 }, .extent = {extent.width, extent.height},
                                       },
                                       0,
                                       level_count,
                                       0,
                                       layer_count,
                                       vulkan_format,
                                       desc.pixel_data.data(), 0);
    /*if (desc.generate_mipmaps)
      ctx.generate_mipmaps(handle);*/
  }
  return Holder{ &ctx, handle };
}

auto
VulkanTextureND::create_image_view(IContext& ctx,
                                   VkFormat override_format,
                                   VkImageAspectFlags aspect,
                                   std::string_view,
                                   std::uint32_t override_level_count,
                                   std::uint32_t override_layer_count,
                                   VkImageViewType override_type,
                                   VkComponentMapping mapping,
                                   std::uint32_t base_level,
                                   std::uint32_t base_layer,
                                   const VkSamplerYcbcrConversionInfo* ycbcr)
  -> VkImageView
{
  const VkImageViewCreateInfo ci = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .pNext = ycbcr,
    .flags = 0,
    .image = image,
    .viewType = override_type,
    .format = override_format,
    .components = mapping,
    .subresourceRange = { aspect,
                          base_level,
                          override_level_count ? override_level_count
                                               : level_count,
                          base_layer,
                          override_layer_count, },
  };
  VkImageView view = VK_NULL_HANDLE;
  vkCreateImageView(ctx.get_device(), &ci, nullptr, &view);
  // lvk::setDebugObjectName(
  //   device, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)vkView, debugName);
  return view;
}
auto
VulkanTextureND::get_or_create_image_view_for_framebuffer(IContext& ctx,
                                                          std::uint8_t level,
                                                          std::uint8_t layer)
  -> VkImageView
{
  auto& cached = framebuffer_image_views.at(level).at(layer);

  if (cached == VK_NULL_HANDLE) {
    cached = create_image_view(
      ctx, format, VK_IMAGE_ASPECT_COLOR_BIT, std::format("Framebuffer[{}{}]", level, layer), 1);
  }

  return cached;
}

}