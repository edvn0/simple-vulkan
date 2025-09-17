#pragma once

#include "common.hpp"
#include "sv/bindless_access.hpp"
#include "vulkan/vulkan_core.h"

#include <cstdint>
#include <vector>

namespace sv {

struct IContext;

template<typename Ctx>
class Bindless final
{
  static constexpr std::uint32_t BINDING_SAMPLED = 0u;
  static constexpr std::uint32_t BINDING_STORAGE = 1u;

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
    auto& desc = BindlessAccess<Ctx>::descriptors(ctx);
    if (desc.layout)
      return;

    VkDescriptorSetLayoutBinding b[2]{};
    b[0] = { BINDING_SAMPLED,
             VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
             sampled_cap,
             VK_SHADER_STAGE_ALL,
             nullptr };
    b[1] = { BINDING_STORAGE,
             VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
             storage_cap,
             VK_SHADER_STAGE_ALL,
             nullptr };

    VkDescriptorBindingFlags bf[2]{
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_ci{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
      nullptr,
      2u,
      bf
    };

    VkDescriptorSetLayoutCreateInfo dsl_ci{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      &flags_ci,
      VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
      2u,
      b
    };

    vkCreateDescriptorSetLayout(
      BindlessAccess<Ctx>::device(ctx), &dsl_ci, nullptr, &desc.layout);
    set_name(ctx,
             desc.layout,
             VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
             "Bindless DescriptorLayout");
    BindlessAccess<Ctx>::enqueue_destruction(
      ctx, [ptr = desc.layout](auto& context) {
        vkDestroyDescriptorSetLayout(context.get_device(), ptr, nullptr);
      });
  }

  static auto allocate_set(Ctx& ctx,
                           std::uint32_t sampled_cap,
                           std::uint32_t storage_cap) -> void
  {
    auto dev = BindlessAccess<Ctx>::device(ctx);
    auto& desc = BindlessAccess<Ctx>::descriptors(ctx);

    if (desc.pool) {
      BindlessAccess<Ctx>::enqueue_destruction(
        ctx, [ptr = desc.pool](auto& context) {
          vkDestroyDescriptorPool(context.get_device(), ptr, nullptr);
        });
      desc.pool = VK_NULL_HANDLE;
      desc.set = VK_NULL_HANDLE;
    }

    VkDescriptorPoolSize sizes[2]{
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampled_cap },
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, storage_cap }
    };

    VkDescriptorPoolCreateInfo dp_ci{
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      nullptr,
      VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
      1u,
      2u,
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
    auto& pool = BindlessAccess<Ctx>::textures(ctx);
    auto& desc = BindlessAccess<Ctx>::descriptors(ctx);

    const auto n = static_cast<std::uint32_t>(pool.size());

    if (n < 1)
      return;

    std::vector<VkDescriptorImageInfo> sampled_infos(n);
    std::vector<VkDescriptorImageInfo> storage_infos(n);

    auto* default_image_view = pool.get(0);

    pool.for_each_dense([&](std::uint32_t i, const auto& v) {
      const auto sampled = v.image_view != VK_NULL_HANDLE
                             ? v.image_view
                             : default_image_view->image_view;
      const auto storage = v.storage_image_view != VK_NULL_HANDLE
                             ? v.storage_image_view
                             : default_image_view->image_view;
      sampled_infos[i] = { VK_NULL_HANDLE,
                           sampled,
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
      storage_infos[i] = { VK_NULL_HANDLE, storage, VK_IMAGE_LAYOUT_GENERAL };
    });

    VkWriteDescriptorSet w[2]{};
    w[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             nullptr,
             desc.set,
             BINDING_SAMPLED,
             0u,
             n,
             VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
             sampled_infos.data(),
             nullptr,
             nullptr };
    w[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             nullptr,
             desc.set,
             BINDING_STORAGE,
             0u,
             n,
             VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
             storage_infos.data(),
             nullptr,
             nullptr };

    vkUpdateDescriptorSets(
      BindlessAccess<Ctx>::device(ctx), 2u, w, 0u, nullptr);
  }

  static auto sync_on_frame_acquire(Ctx& ctx) -> void
  {
    using access = BindlessAccess<Ctx>;
    access::process_pre_frame_work(ctx);

    if (!access::needs_descriptor_update(ctx))
      return;

    auto& desc = access::descriptors(ctx);
    const auto n = static_cast<std::uint32_t>(access::textures(ctx).size());
    const auto cap_samp = std::max(next_pow2(n), 1u);
    const auto cap_store = std::max(next_pow2(n), 1u);

    if (!desc.layout)
      ensure_layout(ctx, cap_samp, cap_store);
    if (desc.set == VK_NULL_HANDLE || cap_samp > desc.sampled_capacity ||
        cap_store > desc.storage_capacity) {
      allocate_set(ctx, cap_samp, cap_store);
    }

    write_all(ctx);
    access::needs_descriptor_update(ctx) = false;
  }
};

}