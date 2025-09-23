#pragma once

#include "common.hpp"
#include "sv/bindless_access.hpp"
#include "sv/texture.hpp"
#include "sv/tracing.hpp"

#include <cstdint>
#include <vector>

namespace sv {

struct IContext;

template<typename Ctx>
class Bindless final
{
  static constexpr std::uint32_t BINDING_SAMPLED = 0u;
  static constexpr std::uint32_t BINDING_STORAGE = 1u;
  static constexpr std::uint32_t BINDING_SAMPLER = 2u;

  static constexpr auto next_pow2(std::uint32_t v) -> std::uint32_t
  {
    if (v <= 1u)
      return 1u;
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return ++v;
  }

public:
  static auto ensure_layout(Ctx& ctx,
                            std::uint32_t sampled_cap,
                            std::uint32_t storage_cap) -> void
  {
    using access = BindlessAccess<Ctx>;
    auto& d = access::descriptors(ctx);
    const bool need_new = !d.layout || sampled_cap > d.sampled_capacity ||
                          storage_cap > d.storage_capacity;
    if (!need_new)
      return;

    if (d.layout) {
      d.layout = VK_NULL_HANDLE;
    }

    VkDescriptorSetLayoutBinding b[3]{};
    b[0] = { 0u,
             VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
             sampled_cap,
             VK_SHADER_STAGE_ALL,
             nullptr };
    b[1] = { 1u,
             VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
             storage_cap,
             VK_SHADER_STAGE_ALL,
             nullptr };
    b[2] = {
      2u, VK_DESCRIPTOR_TYPE_SAMPLER, storage_cap, VK_SHADER_STAGE_ALL, nullptr
    };

    VkDescriptorBindingFlags bf[3]{
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_ci{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
      nullptr,
      std::size(bf),
      bf
    };

    VkDescriptorSetLayoutCreateInfo ci{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      &flags_ci,
      VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
      std::size(b),
      b
    };

    vkCreateDescriptorSetLayout(
      BindlessAccess<Ctx>::device(ctx), &ci, nullptr, &d.layout);
    access::enqueue_destruction(ctx, [ptr = d.layout](auto& c) {
      vkDestroyDescriptorSetLayout(c.get_device(), ptr, nullptr);
    });
    d.sampled_capacity = sampled_cap;
    d.storage_capacity = storage_cap;
  }

  static auto allocate_set(Ctx& ctx,
                           std::uint32_t sampled_cap,
                           std::uint32_t storage_cap) -> void
  {
    auto dev = BindlessAccess<Ctx>::device(ctx);
    auto& desc = BindlessAccess<Ctx>::descriptors(ctx);

    if (desc.pool) {
      desc.pool = VK_NULL_HANDLE;
      desc.set = VK_NULL_HANDLE;
    }

    VkDescriptorPoolSize sizes[3]{
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampled_cap },
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, storage_cap },
      { VK_DESCRIPTOR_TYPE_SAMPLER, storage_cap }
    };

    VkDescriptorPoolCreateInfo dp_ci{
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      nullptr,
      VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
      1u,
      3u,
      sizes
    };

    vkCreateDescriptorPool(dev, &dp_ci, nullptr, &desc.pool);
    set_name(ctx,
             desc.pool,
             VK_OBJECT_TYPE_DESCRIPTOR_POOL,
             "Bindless Descriptor Pool");
    BindlessAccess<Ctx>::enqueue_destruction(
      ctx, [ptr = desc.pool](auto& context) {
        vkDestroyDescriptorPool(context.get_device(), ptr, nullptr);
      });

    VkDescriptorSetAllocateInfo ds_ai{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      nullptr,
      desc.pool,
      1u,
      &desc.layout
    };

    vkAllocateDescriptorSets(dev, &ds_ai, &desc.set);

    desc.sampled_capacity = sampled_cap;
    desc.storage_capacity = storage_cap;
  }

  static auto write_all(Ctx& ctx) -> void
  {
    using access = BindlessAccess<Ctx>;
    auto& pool = access::textures(ctx);
    auto& samplers_pool = access::samplers(ctx);
    auto& desc = access::descriptors(ctx);

    const auto n = static_cast<std::uint32_t>(pool.size());

    if (n < 1)
      return;

    std::vector<VkDescriptorImageInfo> sampled_infos(n);
    std::vector<VkDescriptorImageInfo> storage_infos(n);
    std::vector<VkDescriptorImageInfo> sampler_infos(n);

    auto* default_image_view = pool.get(0);
    auto* default_sampler = samplers_pool.get(0);

    pool.for_each_dense([&](std::uint32_t i, const VulkanTextureND& v) {
      const auto sampled = v.image_view != VK_NULL_HANDLE
                             ? v.image_view
                             : default_image_view->image_view;
      const auto storage = v.storage_image_view != VK_NULL_HANDLE
                             ? v.storage_image_view
                             : default_image_view->image_view;

      const auto is_storage = v.usage_flags & VK_IMAGE_USAGE_STORAGE_BIT;
      const auto is_sampled = v.usage_flags & VK_IMAGE_USAGE_SAMPLED_BIT;

      sampled_infos[i] = { VK_NULL_HANDLE,
                           is_sampled ? sampled
                                      : default_image_view->image_view,
                           VK_IMAGE_LAYOUT_GENERAL };
      storage_infos[i] = { VK_NULL_HANDLE,
                           is_storage ? storage
                                      : default_image_view->image_view,
                           VK_IMAGE_LAYOUT_GENERAL };
      sampler_infos[i] = { *default_sampler,
                           VK_NULL_HANDLE,
                           VK_IMAGE_LAYOUT_UNDEFINED };
    });

    std::vector<VkWriteDescriptorSet> w{};
    std::uint32_t num_writes{ 0 };
    if (!sampled_infos.empty()) {
      auto& write = w.emplace_back();
      write = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        nullptr,
        desc.set,
        BINDING_SAMPLED,
        0u,
        n,
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        sampled_infos.data(),
        nullptr,
        nullptr,
      };
      num_writes++;
    }
    if (!storage_infos.empty()) {
      auto& write = w.emplace_back();

      write = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        nullptr,
        desc.set,
        BINDING_STORAGE,
        0u,
        n,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        storage_infos.data(),
        nullptr,
        nullptr,
      };

      num_writes++;
    }
    if (!sampler_infos.empty()) {
      auto& write = w.emplace_back();

      write = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        nullptr,
        desc.set,
        BINDING_SAMPLER,
        0u,
        n,
        VK_DESCRIPTOR_TYPE_SAMPLER,
        sampler_infos.data(),
        nullptr,
        nullptr,
      };

      num_writes++;
    }

    if (num_writes > 0) {
#if LVK_VULKAN_PRINT_COMMANDS
      LLOGL("vkUpdateDescriptorSets()\n");
#endif // LVK_VULKAN_PRINT_COMMANDS
      access::wait_for_latest(
        ctx); // immediate_->wait(immediate_->getLastSubmitHandle());
      ZoneScopedNC("vkUpdateDescriptorSets()", 0xFF0000);
      vkUpdateDescriptorSets(
        BindlessAccess<Ctx>::device(ctx), num_writes, w.data(), 0u, nullptr);
    }
  }

  static auto sync_on_frame_acquire(Ctx& ctx) -> void
  {
    using access = BindlessAccess<Ctx>;
    access::process_pre_frame_work(ctx);
    if (!access::needs_descriptor_update(ctx))
      return;

    auto& d = access::descriptors(ctx);
    const auto n =
      std::max(std::max(d.sampled_capacity, d.storage_capacity),
               static_cast<std::uint32_t>(access::textures(ctx).size()));
    const auto cap = std::max(next_pow2(n), 1u);

    ensure_layout(ctx, cap, cap);

    if (d.set == VK_NULL_HANDLE || cap > d.sampled_capacity ||
        cap > d.storage_capacity) {
      allocate_set(ctx, cap, cap);
    }

    write_all(ctx);
    access::needs_descriptor_update(ctx) = false;
  }
};

}