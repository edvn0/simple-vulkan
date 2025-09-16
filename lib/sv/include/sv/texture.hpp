#pragma once

#include "sv/common.hpp"
#include "sv/object_holder.hpp"
#include "vulkan/vulkan_core.h"

#include <span>
#include <type_traits>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace sv {

struct TextureDescription
{
  TextureType type{ TextureType::Two };
  Format format{ Format::Invalid };
  Dimensions dimensions{ 1, 1, 1 };
  std::uint32_t layer_count{ 1 };
  std::uint32_t sample_count{ 1 };
  std::uint32_t mip_count{ 1 };
  TextureUsageBits usage_bits{ TextureUsageBits::Sampled };
  StorageType storage{ StorageType::Device };
  ComponentMapping swizzle{};
  ImageTiling tiling{ ImageTiling::Optimal };
  std::span<const std::byte> pixel_data{};
  std::uint32_t mip_count_pixel_data{ 1 };
  bool generate_mipmaps{ false };
  std::string_view debug_name;
};

struct VulkanTextureND
{
  VkImage image{ VK_NULL_HANDLE };
  VkImageUsageFlags usage_flags{ 0 };
  VmaAllocation allocation{ VK_NULL_HANDLE };
  VmaAllocationInfo allocation_info{};
  VkFormatProperties format_properties{};
  VkExtent3D extent{ 0, 0, 0 };
  VkImageType type{ VK_IMAGE_TYPE_MAX_ENUM };
  VkFormat format{ VK_FORMAT_UNDEFINED };
  VkSampleCountFlagBits samples{ VK_SAMPLE_COUNT_1_BIT };
  bool is_swapchain_image{ false };
  bool is_owning_image{ true };
  std::uint32_t level_count{ 1 };
  std::uint32_t layer_count{ 1 };
  bool is_depth_format = false;
  bool is_stencil_format = false;
  mutable VkImageLayout image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkImageView image_view = VK_NULL_HANDLE;         // all levels
  VkImageView storage_image_view = VK_NULL_HANDLE; // identity swizzle
  std::array<std::array<VkImageView, 6>, 8>
    framebuffer_image_views{}; // 6 faces per mip, to a max of 8 mips

  auto create_image_view(IContext&,
    VkFormat format,
    VkImageAspectFlags,
    std::string_view,
    std::uint32_t level_count,
    std::uint32_t layer_count = 1,
    VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D,
    VkComponentMapping mapping = 
      { VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
      },
    std::uint32_t base_level= 0,
    std::uint32_t base_layer= 0,
    const VkSamplerYcbcrConversionInfo* ycbcr = nullptr) -> VkImageView;

  auto get_or_create_image_view_for_framebuffer(IContext& ctx,
                                                std::uint8_t level,
                                                std::uint8_t layer)
    -> VkImageView;

  static auto create(IContext&, const TextureDescription&)
    -> Holder<TextureHandle>;
};

}