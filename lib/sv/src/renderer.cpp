#include "sv/renderer.hpp"

#include "sv/app.hpp"
#include "sv/common.hpp"

#include <GLFW/glfw3.h>
#include <iostream>

namespace sv {

namespace {
auto
full_range_color(VkImage image) -> VkImageMemoryBarrier2
{
  VkImageMemoryBarrier2 b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
  b.image = image;
  b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  b.subresourceRange.baseMipLevel = 0;
  b.subresourceRange.levelCount = 1;
  b.subresourceRange.baseArrayLayer = 0;
  b.subresourceRange.layerCount = 1;
  return b;
}

auto
to_general(VkCommandBuffer buf, VkImage image, VkImageLayout old_layout) -> void
{
  VkImageMemoryBarrier2 b = full_range_color(image);
  b.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
  b.srcAccessMask = 0;
  b.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  b.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  b.oldLayout =
    old_layout; // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR or VK_IMAGE_LAYOUT_UNDEFINED
  b.newLayout = VK_IMAGE_LAYOUT_GENERAL;

  VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
  dep.imageMemoryBarrierCount = 1;
  dep.pImageMemoryBarriers = &b;
  vkCmdPipelineBarrier2(buf, &dep); // replace nullptr with your cmd
}

auto
to_present(VkCommandBuffer buf, VkImage image) -> void
{
  VkImageMemoryBarrier2 b = full_range_color(image);
  b.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  b.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  b.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
  b.dstAccessMask = 0;
  b.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  b.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkDependencyInfo dep{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
  dep.imageMemoryBarrierCount = 1;
  dep.pImageMemoryBarriers = &b;
  vkCmdPipelineBarrier2(buf, &dep); // replace nullptr with your cmd
}

auto
begin_record(VkCommandBuffer cmd, const AcquiredFrame& af) -> void
{
  VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  vkBeginCommandBuffer(cmd, &bi);

  VkRenderingAttachmentInfo color{
    VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO
  };
  color.imageView = af.view;
  color.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.clearValue.color = { .float32 = { 1.0F, 0.0F, 0.0F, 1.0F } };

  VkRenderingInfo ri{ VK_STRUCTURE_TYPE_RENDERING_INFO };
  ri.renderArea = VkRect2D{ { 0, 0 }, af.extent };
  ri.layerCount = 1;
  ri.colorAttachmentCount = 1;
  ri.pColorAttachments = &color;

  vkCmdBeginRendering(cmd, &ri);
}

auto
end_record(VkCommandBuffer cmd) -> void
{
  vkCmdEndRendering(cmd);
  vkEndCommandBuffer(cmd);
}

auto
begin_record_general(VkCommandBuffer cmd, const sv::AcquiredFrame& af) -> void
{
  VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  vkBeginCommandBuffer(cmd, &bi);

  to_general(cmd, af.image, VK_IMAGE_LAYOUT_UNDEFINED);

  VkRenderingAttachmentInfo color{
    VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO
  };
  color.imageView = af.view;
  color.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color.clearValue.color = {};

  VkRenderingInfo ri{ VK_STRUCTURE_TYPE_RENDERING_INFO };
  ri.renderArea = VkRect2D{ { 0, 0 }, af.extent };
  ri.layerCount = 1;
  ri.colorAttachmentCount = 1;
  ri.pColorAttachments = &color;

  vkCmdBeginRendering(cmd, &ri);
}

auto
end_record_general(VkCommandBuffer cmd, const sv::AcquiredFrame& af) -> void
{
  vkCmdEndRendering(cmd);
  to_present(cmd, af.image);
  vkEndCommandBuffer(cmd);
}
}

Renderer::Renderer(IContext& ctx)
  : context(&ctx)
{
}

auto
Renderer::record(const sv::AcquiredFrame& af, VkCommandBuffer cmd) -> void
{
  begin_record_general(cmd, af);

  // bind pipeline(s), draw...

  end_record_general(cmd, af);
}

}