#include "sv/immediate_commands.hpp"

#include "sv/context.hpp"
#include "vulkan/vulkan_core.h"

#include <cassert>
#include <format>
#include <stdexcept>
#include <vulkan/vulkan.h>

namespace sv {

static auto
create_semaphore(const IContext& ctx, const std::string_view name)
{
  assert(!name.empty());
  VkSemaphore semaphore;
  VkSemaphoreCreateInfo create_info{
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
  };
  if (vkCreateSemaphore(ctx.get_device(), &create_info, nullptr, &semaphore) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create semaphore");
  }
  set_name(ctx, semaphore, VK_OBJECT_TYPE_SEMAPHORE, "{}", name);
  return semaphore;
}

static auto
create_fence(const IContext& ctx, const std::string_view name)
{
  assert(!name.empty());
  VkFence fence;
  VkFenceCreateInfo create_info{
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
  };
  if (vkCreateFence(ctx.get_device(), &create_info, nullptr, &fence) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create fence");
  }
  set_name(ctx, fence, VK_OBJECT_TYPE_FENCE, "{}", name);
  return fence;
}

ImmediateCommands::ImmediateCommands(IContext& ctx, std::string_view debug_name)
  : context(&ctx)
  , queue_family_index(ctx.get_graphics_queue_family())
  , debug_name(debug_name)
{
  const VkCommandPoolCreateInfo pool_info{
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .pNext = nullptr,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
             VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
    .queueFamilyIndex = queue_family_index,
  };
  vkCreateCommandPool(ctx.get_device(), &pool_info, nullptr, &command_pool);
  set_name(ctx,
           command_pool,
           VK_OBJECT_TYPE_COMMAND_POOL,
           "CommandPool::{}",
           debug_name);

  const VkCommandBufferAllocateInfo ai = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .pNext = nullptr,
    .commandPool = command_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };
  for (auto i = 0U; i < max_command_buffers; i++) {
    auto& buf = command_buffers[i];
    buf.semaphore =
      create_semaphore(ctx, std::format("Semaphore_{}::{}", i, debug_name));
    buf.fence = create_fence(ctx, std::format("Fence_{}::{}", i, debug_name));
    vkAllocateCommandBuffers(
      ctx.get_device(), &ai, &buf.command_buffer_allocated);
    set_name(ctx,
             buf.command_buffer_allocated,
             VK_OBJECT_TYPE_COMMAND_BUFFER,
             "CommandBuffer_{}::{}",
             i,
             debug_name);
    buf.handle.buffer_index = i;
  }
}

ImmediateCommands::~ImmediateCommands()
{
  wait_all();
  for (auto& buf : command_buffers) {
    vkDestroyFence(context->get_device(), buf.fence, nullptr);
    vkDestroySemaphore(context->get_device(), buf.semaphore, nullptr);
  }
  vkDestroyCommandPool(context->get_device(), command_pool, nullptr);
}

auto
ImmediateCommands::acquire() -> CommandBufferWrapper&
{
  while (!available_command_buffers)
    purge();

  CommandBufferWrapper* current = nullptr;
  for (auto& buf : command_buffers) {
    if (buf.command_buffer == VK_NULL_HANDLE) {
      current = &buf;
      break;
    }
  }

  if (!current) {
    throw std::runtime_error("No available command buffers");
  }

  current->handle.submit_identifier = submit_counter;
  available_command_buffers--;
  current->command_buffer = current->command_buffer_allocated;
  current->is_encoding = true;

  constexpr VkCommandBufferBeginInfo begin_info{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .pNext = nullptr,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    .pInheritanceInfo = nullptr,
  };
  vkBeginCommandBuffer(current->command_buffer, &begin_info);
  next_submit_handle = current->handle;
  return *current;
}

auto
ImmediateCommands::purge() -> void
{
  const auto num_buffers = command_buffers.size();
  for (auto i = 0U; i < num_buffers; i++) {
    const auto index = i + last_submit_handle.buffer_index + 1;
    auto& buffer = command_buffers[index % num_buffers];
    if (buffer.command_buffer == VK_NULL_HANDLE || buffer.is_encoding)
      continue;

    const auto result =
      vkWaitForFences(context->get_device(), 1, &buffer.fence, VK_TRUE, 0);
    if (result == VK_SUCCESS) {
      vkResetCommandBuffer(buffer.command_buffer, 0);
      vkResetFences(context->get_device(), 1, &buffer.fence);
      buffer.command_buffer = VK_NULL_HANDLE;
      available_command_buffers++;
    } else {
      if (result != VK_TIMEOUT)
        throw std::runtime_error("Failed to wait for fence");
    }
  }
}

auto
ImmediateCommands::submit(const CommandBufferWrapper& wrapper) -> SubmitHandle
{
  vkEndCommandBuffer(wrapper.command_buffer);
  std::array<VkSemaphoreSubmitInfo, 2> wait_semaphores{
    VkSemaphoreSubmitInfo{},
    VkSemaphoreSubmitInfo{},
  };
  std::uint32_t wait_semaphore_count = 0;
  if (wait_semaphore_info.semaphore != VK_NULL_HANDLE) {
    wait_semaphores[wait_semaphore_count++] = wait_semaphore_info;
  }
  if (last_submit_semaphore.semaphore != VK_NULL_HANDLE) {
    wait_semaphores[wait_semaphore_count++] = last_submit_semaphore;
  }

  std::array<VkSemaphoreSubmitInfo, 2> signal_semaphores{
    VkSemaphoreSubmitInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .pNext = nullptr,
      .semaphore = wrapper.semaphore,
      .value = 0,
      .stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      .deviceIndex = 0,
    },
    {},
  };
  auto signal_semaphore_count = 1U;
  if (signal_semaphore_info.semaphore != VK_NULL_HANDLE) {
    signal_semaphores[signal_semaphore_count++] = signal_semaphore_info;
  }

  const VkCommandBufferSubmitInfo bufferSI = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
    .pNext = nullptr,
    .commandBuffer = wrapper.command_buffer,
    .deviceMask = 0,
  };
  const VkSubmitInfo2 si = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
    .pNext = nullptr,
    .flags = 0,
    .waitSemaphoreInfoCount = wait_semaphore_count,
    .pWaitSemaphoreInfos = wait_semaphores.data(),
    .commandBufferInfoCount = 1u,
    .pCommandBufferInfos = &bufferSI,
    .signalSemaphoreInfoCount = signal_semaphore_count,
    .pSignalSemaphoreInfos = signal_semaphores.data(),
  };
  vkQueueSubmit2(context->get_graphics_queue(), 1u, &si, wrapper.fence);
  last_submit_semaphore.semaphore = wrapper.semaphore;
  last_submit_handle = wrapper.handle;

  wait_semaphore_info.semaphore = VK_NULL_HANDLE;
  signal_semaphore_info.semaphore = VK_NULL_HANDLE;
  const_cast<CommandBufferWrapper&>(wrapper).is_encoding = false;
  submit_counter++;
  if (!submit_counter)
    submit_counter++;
  return last_submit_handle;
}

auto
ImmediateCommands::is_ready(SubmitHandle handle) const -> bool
{
  if (handle.empty())
    return true;

  const auto& buf = command_buffers[handle.buffer_index];
  if (buf.command_buffer == VK_NULL_HANDLE)
    return false;

  if (buf.handle.submit_identifier != handle.submit_identifier)
    return true;
  return vkWaitForFences(context->get_device(), 1, &buf.fence, VK_TRUE, 0) ==
         VK_SUCCESS;
}

auto
ImmediateCommands::wait(const SubmitHandle handle) -> void
{
  if (handle.empty()) {
    vkDeviceWaitIdle(context->get_device());
    return;
  }
  if (is_ready(handle))
    return;
  if (!command_buffers[handle.buffer_index].is_encoding)
    return;
  vkWaitForFences(context->get_device(),
                  1,
                  &command_buffers[handle.buffer_index].fence,
                  VK_TRUE,
                  UINT64_MAX);
  purge();
}

auto
ImmediateCommands::wait_all() -> void
{
  std::array<VkFence, max_command_buffers> fences{};
  auto fence_count = 0U;
  for (const auto& wrapper : command_buffers) {
    if (wrapper.command_buffer != VK_NULL_HANDLE && !wrapper.is_encoding)
      fences[fence_count++] = wrapper.fence;
  }
  if (fence_count > 0) {
    vkWaitForFences(
      context->get_device(), fence_count, fences.data(), VK_TRUE, UINT64_MAX);
  }
  purge();
}

auto
ImmediateCommands::wait_semaphore(VkSemaphore s) -> void
{
  assert(wait_semaphore_info.semaphore == VK_NULL_HANDLE);
  wait_semaphore_info.semaphore = s;
}

void
ImmediateCommands::signal_semaphore(VkSemaphore semaphore,
                                    std::uint64_t signalValue)
{
  assert(signal_semaphore_info.semaphore == VK_NULL_HANDLE);

  signal_semaphore_info.semaphore = semaphore;
  signal_semaphore_info.value = signalValue;
}

auto
ImmediateCommands::acquire_last_submit_semaphore() -> VkSemaphore
{
  return std::exchange(last_submit_semaphore.semaphore, VK_NULL_HANDLE);
}

} // namespace sv