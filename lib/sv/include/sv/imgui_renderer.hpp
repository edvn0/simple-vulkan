#pragma once

#include "sv/abstract_command_buffer.hpp"
#include "sv/common.hpp"
#include "sv/object_holder.hpp"

#include <array>
#include <string_view>

namespace sv {

class ImGuiRenderer
{
  IContext* context;
  Holder<ShaderModuleHandle> gui_shader;
  Holder<GraphicsPipelineHandle> graphics_pipeline;
  Holder<TextureHandle> font_texture;
  Holder<SamplerHandle> sampler_clamp_to_edge;
  float display_scale{ 1.0F };
  std::uint32_t frame_index{ 0 };

  struct Drawable
  {
    Holder<BufferHandle> vertex_buffer;
    Holder<BufferHandle> index_buffer;
    std::uint32_t allocated_indices{ 0 };
    std::uint32_t allocated_vertices{ 0 };
  };

  static constexpr auto max_drawables = 3U;
  std::array<Drawable, max_drawables> drawables{};

  auto create_pipeline(const Framebuffer&) const
    -> Holder<GraphicsPipelineHandle>;

public:
  explicit ImGuiRenderer(IContext&,
                         std::string_view default_font_ttf = {},
                         float font_size = 24.0F);
  ~ImGuiRenderer();

  auto update_font(std::string_view, float) -> void;

  auto begin_frame(const Framebuffer&) -> void;
  auto end_frame(ICommandBuffer&) -> void;
  auto set_display_scale(float) -> void;
};

}