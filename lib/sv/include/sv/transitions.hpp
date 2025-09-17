#pragma once

#include <optional>
#include <vulkan/vulkan.h>

namespace sv {

class ImageTransition
{
public:
  struct LayoutInfo
  {
    VkPipelineStageFlags2 stage_mask;
    VkAccessFlags2 access_mask;
  };

  static auto get_layout_info(VkImageLayout layout,
                              bool is_color_attachment = false) -> LayoutInfo
  {
    switch (layout) {
      case VK_IMAGE_LAYOUT_UNDEFINED:
        return { VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE };

      case VK_IMAGE_LAYOUT_GENERAL:
        // For color attachments, we can be more specific about expected usage
        if (is_color_attachment) {
          return { VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT |
                     VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                   VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
                     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
                     VK_ACCESS_2_SHADER_READ_BIT |
                     VK_ACCESS_2_SHADER_WRITE_BIT };
        } else {
          return { VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                   VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT };
        }

      case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return { VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                 VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
                   VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT };

      case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        return { VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                   VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                 VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                   VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT };

      case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
        return { VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                   VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT |
                   VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                   VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                 VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                   VK_ACCESS_2_SHADER_READ_BIT };

      case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return { VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                   VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                 VK_ACCESS_2_SHADER_READ_BIT };

      case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return { VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                 VK_ACCESS_2_TRANSFER_READ_BIT };

      case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return { VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                 VK_ACCESS_2_TRANSFER_WRITE_BIT };

      case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        return { VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_NONE };

      default:
        return { VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                 VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT };
    }
  }

  // Simple transition with automatic stage/access deduction
  static auto transition_layout(
    VkCommandBuffer cmd_buffer,
    VkImage image,
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    const VkImageSubresourceRange& subresource_range = default_color_range(),
    bool is_color_attachment = false) -> void
  {

    auto src_info = get_layout_info(old_layout, is_color_attachment);
    auto dst_info = get_layout_info(new_layout, is_color_attachment);

    const VkImageMemoryBarrier2 barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .pNext = nullptr,
      .srcStageMask = src_info.stage_mask,
      .srcAccessMask = src_info.access_mask,
      .dstStageMask = dst_info.stage_mask,
      .dstAccessMask = dst_info.access_mask,
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = subresource_range,
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

    vkCmdPipelineBarrier2(cmd_buffer, &dependency_info);
  }

  // Convenience method for color images with default subresource range
  static auto transition_color(VkCommandBuffer cmd_buffer,
                               VkImage image,
                               VkImageLayout old_layout,
                               VkImageLayout new_layout) -> void
  {
    transition_layout(
      cmd_buffer, image, old_layout, new_layout, default_color_range(), true);
  }

  // Convenience method for depth images
  static auto transition_depth(VkCommandBuffer cmd_buffer,
                               VkImage image,
                               VkImageLayout old_layout,
                               VkImageLayout new_layout) -> void
  {
    transition_layout(
      cmd_buffer, image, old_layout, new_layout, default_depth_range());
  }

  // Helper for swapchain images (color attachment usage)
  static auto transition_swapchain(VkCommandBuffer cmd_buffer,
                                   VkImage image,
                                   VkImageLayout old_layout,
                                   VkImageLayout new_layout) -> void
  {
    transition_color(cmd_buffer, image, old_layout, new_layout);
  }

  // Common transition patterns as static methods
  static auto undefined_to_color_attachment(VkCommandBuffer cmd_buffer,
                                            VkImage image) -> void
  {
    transition_color(cmd_buffer,
                     image,
                     VK_IMAGE_LAYOUT_UNDEFINED,
                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  }

  static auto undefined_to_shader_read(VkCommandBuffer cmd_buffer,
                                       VkImage image) -> void
  {
    transition_color(cmd_buffer,
                     image,
                     VK_IMAGE_LAYOUT_UNDEFINED,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }

  static auto color_attachment_to_shader_read(VkCommandBuffer cmd_buffer,
                                              VkImage image) -> void
  {
    transition_color(cmd_buffer,
                     image,
                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }

  static auto shader_read_to_color_attachment(VkCommandBuffer cmd_buffer,
                                              VkImage image) -> void
  {
    transition_color(cmd_buffer,
                     image,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  }


  static auto undefined_to_transfer_dst(VkCommandBuffer cmd_buffer,
                                        VkImage image) -> void
  {
    transition_color(cmd_buffer,
                     image,
                     VK_IMAGE_LAYOUT_UNDEFINED,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  }

  static auto transfer_dst_to_shader_read(VkCommandBuffer cmd_buffer,
                                          VkImage image) -> void
  {
    transition_color(cmd_buffer,
                     image,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }

  // For cases where you need custom stages/access (escape hatch)
  static auto transition_custom(
    VkCommandBuffer cmd_buffer,
    VkImage image,
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    VkPipelineStageFlags2 src_stage,
    VkAccessFlags2 src_access,
    VkPipelineStageFlags2 dst_stage,
    VkAccessFlags2 dst_access,
    const VkImageSubresourceRange& subresource_range = default_color_range())
    -> void
  {

    const VkImageMemoryBarrier2 barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .pNext = nullptr,
      .srcStageMask = src_stage,
      .srcAccessMask = src_access,
      .dstStageMask = dst_stage,
      .dstAccessMask = dst_access,
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = subresource_range,
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

    vkCmdPipelineBarrier2(cmd_buffer, &dependency_info);
  }


  static auto present_to_color_attachment(VkCommandBuffer cmd_buffer,
                                          VkImage image) -> void
  {
    transition_custom(cmd_buffer,
                      image,
                      VK_IMAGE_LAYOUT_UNDEFINED,
                      VK_IMAGE_LAYOUT_GENERAL,
                      VK_PIPELINE_STAGE_2_NONE,
                      VK_ACCESS_2_NONE,
                      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                      VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
                        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
  }

  static auto color_attachment_to_present(VkCommandBuffer cmd_buffer,
                                          VkImage image) -> void
  {
    transition_custom(cmd_buffer,
                      image,
                      VK_IMAGE_LAYOUT_GENERAL,
                      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_NONE,
                      VK_ACCESS_2_NONE);
  }

private:
  static auto default_color_range() -> VkImageSubresourceRange
  {
    return VkImageSubresourceRange{
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = VK_REMAINING_MIP_LEVELS,
      .baseArrayLayer = 0,
      .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };
  }

  static auto default_depth_range() -> VkImageSubresourceRange
  {
    return VkImageSubresourceRange{
      .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
      .baseMipLevel = 0,
      .levelCount = VK_REMAINING_MIP_LEVELS,
      .baseArrayLayer = 0,
      .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };
  }
};

namespace Transition {
inline auto
image(VkCommandBuffer cmd_buffer,
      VkImage image,
      VkImageLayout old_layout,
      VkImageLayout new_layout) -> void
{
  ImageTransition::transition_layout(cmd_buffer, image, old_layout, new_layout);
}

inline auto
depth_image(VkCommandBuffer cmd_buffer,
            VkImage image,
            VkImageLayout old_layout,
            VkImageLayout new_layout) -> void
{
  ImageTransition::transition_depth(cmd_buffer, image, old_layout, new_layout);
}

inline auto
swapchain_image(VkCommandBuffer cmd_buffer,
                VkImage image,
                VkImageLayout old_layout,
                VkImageLayout new_layout) -> void
{
  ImageTransition::transition_swapchain(
    cmd_buffer, image, old_layout, new_layout);
}

inline auto
acquire_swapchain_for_rendering(VkCommandBuffer cmd_buffer, VkImage image)
  -> void
{
  ImageTransition::present_to_color_attachment(cmd_buffer, image);
}
inline auto
release_swapchain_for_present(VkCommandBuffer cmd_buffer, VkImage image) -> void
{
  ImageTransition::color_attachment_to_present(cmd_buffer, image);
}
}

} // namespace VkBindless