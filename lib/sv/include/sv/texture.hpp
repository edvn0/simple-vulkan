#pragma once

#include "object_handle.hpp"
#include "sv/common.hpp"
#include "sv/object_holder.hpp"
#include "vulkan/vulkan_core.h"

#include <span>
#include <type_traits>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace sv {

static constexpr auto max_mip_levels_framebuffer = 8ULL;
static constexpr auto num_faces_cube = 6ULL;
static constexpr auto max_layers_framebuffer = num_faces_cube;

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
  std::string debug_name{};
  mutable VkImageLayout image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkImageView image_view = VK_NULL_HANDLE;         // all levels
  VkImageView storage_image_view = VK_NULL_HANDLE; // identity swizzle
  std::array<std::array<VkImageView, max_layers_framebuffer>,
             max_mip_levels_framebuffer>
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

  auto swap(VulkanTextureND& other) noexcept -> void
  {
    using std::swap;
    swap(image, other.image);
    swap(image_view, other.image_view);
    swap(storage_image_view, other.storage_image_view);
    swap(allocation, other.allocation);
    swap(allocation_info, other.allocation_info);
    swap(extent, other.extent);
    swap(type, other.type);
    swap(format, other.format);
    swap(samples, other.samples);
    swap(level_count, other.level_count);
    swap(layer_count, other.layer_count);
    swap(usage_flags, other.usage_flags);
    swap(format_properties, other.format_properties);
    swap(is_depth_format, other.is_depth_format);
    swap(is_stencil_format, other.is_stencil_format);
    swap(is_owning_image, other.is_owning_image);
    swap(debug_name, other.debug_name);
    swap(framebuffer_image_views, other.framebuffer_image_views);
  }

  inline auto swap(VulkanTextureND& a, VulkanTextureND& b) noexcept -> void
  {
    a.swap(b);
  }

  static auto create(IContext&, const TextureDescription&)
    -> Holder<TextureHandle>;

  static auto build(IContext&, const TextureDescription&) -> VulkanTextureND;

  static auto create(IContext&, const VkSamplerCreateInfo&)
    -> Holder<SamplerHandle>;
};

}