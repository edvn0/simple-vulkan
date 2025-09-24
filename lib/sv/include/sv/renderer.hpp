#pragma once

#include "glm/matrix.hpp"
#include "sv/abstract_context.hpp"
#include "sv/app.hpp"
#include "sv/buffer.hpp"
#include "sv/common.hpp"
#include "sv/imgui_renderer.hpp"
#include "sv/line_canvas.hpp"
#include "sv/object_handle.hpp"
#include "sv/object_holder.hpp"

#include <vector>
#include <vulkan/vulkan.h>

namespace sv {

struct IContext;
class Camera;

template<typename T>
class FrameCountBuffer
{
  IContext* context{ nullptr };
  std::vector<Holder<BufferHandle>> buffers;
  std::size_t count{};

public:
  FrameCountBuffer(IContext& ctx, const std::size_t c)
    : context(&ctx)
    , count(c)
  {
    buffers.resize(count);
    for (auto& b : buffers) {
      b = VulkanDeviceBuffer::create(ctx,
                                     BufferDescription{
                                       .data = {},
                                       .usage = BufferUsageBits::Uniform,
                                       .storage = StorageType::HostVisible,
                                       .size = sizeof(T),
                                       .debug_name = "FrameCountBuffer",
                                     });
    }
  }

  auto get(std::uint32_t frame_index) -> std::uint64_t
  {
    return context->get_buffer_pool()
      .get(*buffers.at(frame_index % count))
      ->get_device_address();
  }

  auto upload(const std::uint32_t frame_index,
              const T& t,
              const std::size_t offset = 0) -> void
  {
    context->get_buffer_pool()
      .get(*buffers.at(frame_index % count))
      ->upload(std::as_bytes(std::span{ &t, 1 }), offset);
  }
};

struct ShadowCascadeData
{
  glm::mat4 view{};
  glm::mat4 proj{};
  glm::mat4 vp{};
};
struct ShadowUBOData
{
  std::array<ShadowCascadeData, 8> cascades{};
  std::uint32_t cascade_count{};
  std::uint32_t pad0{}, pad1{}, pad2{};
};
struct ShadowSplits
{
  std::array<float, 8> half_extents{};
  std::uint32_t count{};
};

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

  struct Tonemap
  {
    Holder<ShaderModuleHandle> shader{};
    Holder<GraphicsPipelineHandle> pipeline{};
  };

  struct GBuffer
  {
    Holder<ShaderModuleHandle> shader;
    Holder<GraphicsPipelineHandle> pipeline;
    Holder<TextureHandle>
      oct_normals_extras_tbd{};          // r8g8 normals, b8a8 extras tbd
    Holder<TextureHandle> material_id{}; // u32 red
    Holder<TextureHandle> uvs{};         // u32 red
    Holder<TextureHandle> depth_32{};    // d32 with stencil maybe?
  };
  struct GBufferLighting
  {
    Holder<TextureHandle> hdr{};
    Holder<ShaderModuleHandle> shader;
    Holder<GraphicsPipelineHandle> pipeline;
  };
  struct DirectionalShadow
  {
    Holder<TextureHandle> texture{};
    Holder<SamplerHandle> sampler{};
    Holder<ShaderModuleHandle> shader{};
    Holder<GraphicsPipelineHandle> pipeline{};
  };

  struct Grid
  {
    Holder<ShaderModuleHandle> shader{};
    Holder<GraphicsPipelineHandle> pipeline{};
  };

  struct Impl;
  std::unique_ptr<Impl, PimplDeleter> impl;

  std::tuple<std::uint32_t, std::uint32_t> deferred_extent{};
  GBuffer deferred_mrt;                 // Phase 1
  DirectionalShadow directional_shadow; // Phase 1*
  GBufferLighting deferred_hdr_gbuffer; // Phase 2
  Grid grid;                            // Phase 3
  LineCanvas3D canvas_3d;               // Phase 3
  Tonemap tonemap;                      // Phase 4
  std::unique_ptr<ImGuiRenderer> imgui; // Phase 5

  struct UBO
  {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 view_proj;
    glm::mat4 inverse_view;
    glm::mat4 inverse_projection;
    glm::mat4 inverse_view_proj;
    glm::vec4 light_direction{};
    glm::vec4 camera_position{};
  };
  float rad_phi = glm::radians(-37.76f);   // ≈ -0.659 rad
  float rad_theta = glm::radians(126.16f); // ≈  2.202 rad
  FrameCountBuffer<UBO> ubo;
  auto create_ubo(const glm::mat4& view, const glm::mat4& proj) -> UBO
  {
    return {
      .view = view,
      .projection = proj,
      .view_proj = proj * view,
      .inverse_view = glm::inverse(view),
      .inverse_projection = glm::inverse(proj),
      .inverse_view_proj = glm::inverse(view) * glm::inverse(proj),
    };
  }

  ShadowSplits shadow_splits{
    .half_extents = { 10.f, 25.f, 60.f, 140.f },
    .count = 4,
  };
  float shadow_near = 1.f;
  float shadow_far = 300.f;
  FrameCountBuffer<ShadowUBOData> shadow_ubo;
  auto update_shadow_ubo_layers(const glm::vec3&) -> void;
  auto build_centered_cascades(const glm::vec3& light_dir,
                               const ShadowSplits& splits,
                               float z_near,
                               float z_far) -> ShadowUBOData;
  std::uint32_t current_frame{ 0 };

public:
  Renderer(IContext&, const std::tuple<std::uint32_t, std::uint32_t>& extent);
  ~Renderer() override;
  auto begin_frame(const Camera&) -> void;
  auto record(ICommandBuffer&, TextureHandle) -> void override;
  auto resize(std::uint32_t, std::uint32_t) -> void override;
};

}