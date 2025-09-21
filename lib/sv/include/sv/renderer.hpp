#pragma once

#include "object_handle.hpp"
#include "sv/abstract_context.hpp"
#include "sv/object_holder.hpp"

#include <vulkan/vulkan.h>

namespace sv {

struct IContext;

struct IRenderer
{
  virtual ~IRenderer() = default;
  virtual auto record(ICommandBuffer&, TextureHandle) -> void = 0;
  virtual auto resize(std::uint32_t, std::uint32_t) -> void = 0;
};

class Renderer : public IRenderer
{
private:
  IContext* context{ nullptr };

  Holder<GraphicsPipelineHandle> basic;
  Holder<ShaderModuleHandle> basic_shader;

  struct GBuffer
  {
    Holder<ShaderModuleHandle> shader;
    Holder<GraphicsPipelineHandle> pipeline;
    Holder<TextureHandle> oct_normals_extras_tbd{}; // r8g8 normals, b8a8 extras tbd
    Holder<TextureHandle> material_id{};            // u32 red
    Holder<TextureHandle> depth_32{};               // d32 with stencil maybe?


  };
  struct GBufferLighting
  {
    Holder<TextureHandle> hdr{};
    Holder<ShaderModuleHandle> shader;
    Holder<GraphicsPipelineHandle> pipeline;
  };

  struct Impl;
  std::unique_ptr<Impl, PimplDeleter> impl;

  GBuffer deferred_mrt;
  std::tuple<std::uint32_t, std::uint32_t> deferred_extent{};
  GBufferLighting deferred_hdr_gbuffer;


public:
  Renderer(IContext&, const std::tuple<std::uint32_t, std::uint32_t>& extent);
  ~Renderer() override = default;
  auto record(ICommandBuffer&, TextureHandle) -> void override;
  auto resize(std::uint32_t, std::uint32_t) -> void override;
};

}