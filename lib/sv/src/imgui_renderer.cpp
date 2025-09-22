#include "sv/imgui_renderer.hpp"

#include "sv/buffer.hpp"
#include "sv/common.hpp"
#include "sv/context.hpp"
#include "sv/pipeline.hpp"
#include "sv/texture.hpp"

#include <cmath>
#include <imgui.h>
#if defined(WITH_IMPLOT)
#include <implot.h>
#endif

#if defined(WITH_IMGUIZMO)
#include <ImGuizmo.h>
#endif

namespace sv {

auto
ImGuiRenderer::create_pipeline(const Framebuffer& fb) const
  -> Holder<GraphicsPipelineHandle>
{
  const auto& colour_space = dynamic_cast<VulkanContext*>(context)
                               ->get_swapchain()
                               .swapchain.color_space;
  const std::uint32_t is_non_linear_colour_space =
    colour_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ||
    colour_space == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;

  const auto span = std::span{ &is_non_linear_colour_space, 1 };
  return VulkanGraphicsPipeline::create(*context, {
  .shader = *gui_shader,
    .specialisation_constants = {
      .entries={
        SpecialisationConstantDescription::SpecialisationConstantEntry{
          .constant_id = 0,
          .offset = 0,
          .size = sizeof(std::uint32_t),
        },
      },
      .data = std::as_bytes(span),
  },
    .color = {
      ColourAttachment{
        .format = get_format<Format>(*context, fb.color.at(0).texture),
        .blend_enabled = true,
        .src_rgb_blend_factor = BlendFactor::SrcAlpha,
        .dst_rgb_blend_factor = BlendFactor::OneMinusSrcAlpha,
        },
    },
    .depth_format = fb.depth_stencil.texture.valid() ?
      get_format<Format>(*context, fb.depth_stencil.texture) :
      Format::Invalid,
    .cull_mode = CullMode::None,
    .debug_name = "ImGui"
  });
}

ImGuiRenderer::ImGuiRenderer(IContext& ctx,
                             const std::string_view default_font_ttf,
                             const float font_size)
  : context(&ctx)
{
  ImGui::CreateContext();
#if defined(WITH_IMPLOT)
  ImPlot::CreateContext();
#endif

  ImGuiIO& io = ImGui::GetIO();
  io.BackendRendererName = "imgui-vk-simple";
  io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

  update_font(default_font_ttf, font_size);
  gui_shader = VulkanShader::create(*context, "shaders/gui.shader");
  sampler_clamp_to_edge = VulkanTextureND::create(
    *context,
    VkSamplerCreateInfo{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .mipLodBias = 0.0F,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 0.0F,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_ALWAYS,
      .minLod = 0.0F,
      .maxLod = 1.0F,
      .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
    });
}

ImGuiRenderer::~ImGuiRenderer()
{
  const auto& io = ImGui::GetIO();
  io.Fonts->TexID = nullptr;
#if defined(WITH_IMPLOT)
  ImPlot::DestroyContext();
#endif
  ImGui::DestroyContext();
}

auto
ImGuiRenderer::begin_frame(const Framebuffer& desc) -> void
{
  const auto dim =
    context->get_texture_pool().get(desc.color.at(0).texture)->extent;
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(static_cast<float>(dim.width) / display_scale,
                          static_cast<float>(dim.height) / display_scale);
  io.DisplayFramebufferScale = ImVec2(display_scale, display_scale);
  io.IniFilename = nullptr;
  if (graphics_pipeline.empty()) {
    graphics_pipeline = create_pipeline(desc);
  }
  ImGui::NewFrame();
#if defined(WITH_IMGUIZMO)
  ImGuizmo::BeginFrame();
#endif
}

void
ImGuiRenderer::end_frame(ICommandBuffer& command_buffer)
{
  ImGui::EndFrame();
  ImGui::Render();
  ImDrawData* dd = ImGui::GetDrawData();
  const float fb_width = dd->DisplaySize.x * dd->FramebufferScale.x;
  const float fb_height = dd->DisplaySize.y * dd->FramebufferScale.y;

  command_buffer.cmd_bind_depth_state({});
  command_buffer.cmd_bind_viewport({
    .x = 0.0f,
    .y = 0.0f,
    .width = fb_width,
    .height = fb_height,
  });

  const float L = dd->DisplayPos.x;
  const float R = dd->DisplayPos.x + dd->DisplaySize.x;
  const float T = dd->DisplayPos.y;
  const float B = dd->DisplayPos.y + dd->DisplaySize.y;
  const ImVec2 clipOff = dd->DisplayPos;
  const ImVec2 clipScale = dd->FramebufferScale;

  auto& drawable = drawables.at(frame_index);
  frame_index = (frame_index + 1) % std::size(drawables);

  if (drawable.allocated_indices <
      static_cast<std::uint32_t>(dd->TotalIdxCount)) {
    drawable.index_buffer = VulkanDeviceBuffer::create(
      *context,
      {
        .usage = BufferUsageBits::Index,
        .storage = StorageType::HostVisible,
        .size = dd->TotalIdxCount * sizeof(ImDrawIdx),
        .debug_name = "ImGui_drawable_data.index_buffer",
      });
    drawable.allocated_indices = dd->TotalIdxCount;
  }

  if (drawable.allocated_vertices <
      static_cast<std::uint32_t>(dd->TotalVtxCount)) {
    drawable.vertex_buffer = VulkanDeviceBuffer::create(
      *context,
      {
        .usage = BufferUsageBits::Storage,
        .storage = StorageType::HostVisible,
        .size = dd->TotalVtxCount * sizeof(ImDrawVert),
        .debug_name = "ImGui_drawable_data.vertex_buffer",
      });
    drawable.allocated_vertices = dd->TotalVtxCount;
  }

  if (drawable.allocated_vertices == 0 || drawable.allocated_indices == 0) {
    return;
  }

  auto vtx = get_mapped_pointer<ImDrawVert>(*context, *drawable.vertex_buffer);
  auto idx =
    get_mapped_pointer<std::uint16_t>(*context, *drawable.index_buffer);
  for (int n = 0; n < dd->CmdListsCount; n++) {
    const ImDrawList* command_list = dd->CmdLists[n];
    std::memcpy(vtx,
                command_list->VtxBuffer.Data,
                command_list->VtxBuffer.Size * sizeof(ImDrawVert));
    std::memcpy(idx,
                command_list->IdxBuffer.Data,
                command_list->IdxBuffer.Size * sizeof(ImDrawIdx));
    vtx += command_list->VtxBuffer.Size;
    idx += command_list->IdxBuffer.Size;
  }

  context->flush_mapped_memory(*drawable.vertex_buffer,
                               {
                                 0,
                                 dd->TotalVtxCount * sizeof(ImDrawVert),
                               });
  context->flush_mapped_memory(*drawable.index_buffer,
                               {
                                 0,
                                 dd->TotalIdxCount * sizeof(ImDrawIdx),
                               });

  std::uint32_t index_offset = 0;
  std::uint32_t vertex_offset = 0;
  command_buffer.cmd_bind_index_buffer(
    *drawable.index_buffer, IndexFormat::UI16, 0);
  command_buffer.cmd_bind_graphics_pipeline(*graphics_pipeline);
  for (std::int32_t n = 0; n < dd->CmdListsCount; n++) {
    const auto* command_list = dd->CmdLists[n];
    for (std::int32_t index = 0; index < command_list->CmdBuffer.Size;
         index++) {
      const auto& cmd = command_list->CmdBuffer[index];
      ImVec2 clip_min((cmd.ClipRect.x - clipOff.x) * clipScale.x,
                      (cmd.ClipRect.y - clipOff.y) * clipScale.y);
      ImVec2 clip_max((cmd.ClipRect.z - clipOff.x) * clipScale.x,
                      (cmd.ClipRect.w - clipOff.y) * clipScale.y);
      if (clip_min.x < 0.0f)
        clip_min.x = 0.0f;
      if (clip_min.y < 0.0f)
        clip_min.y = 0.0f;
      if (clip_max.x > fb_width)
        clip_max.x = fb_width;
      if (clip_max.y > fb_height)
        clip_max.y = fb_height;
      if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
        continue;
      struct VulkanImguiBindData
      {
        std::array<float, 4> LRTB{ { L, R, T, B } };
        std::uint32_t texture_index = 0;
        std::uint32_t sampler_index = 0;
        std::uint64_t vb = 0;
      } bind_data = {
        .texture_index = static_cast<std::uint32_t>(cmd.TexRef.GetTexID()),
        .sampler_index = sampler_clamp_to_edge.index(),
        .vb = context->get_buffer_pool()
                .get(*drawable.vertex_buffer)
                ->get_device_address(),
      };
      command_buffer.cmd_push_constants<VulkanImguiBindData>(bind_data, 0);
      command_buffer.cmd_bind_scissor_rect({
        static_cast<uint32_t>(clip_min.x),
        static_cast<uint32_t>(clip_min.y),
        static_cast<uint32_t>(clip_max.x - clip_min.x),
        static_cast<uint32_t>(clip_max.y - clip_min.y),
      });
      command_buffer.cmd_draw_indexed(
        cmd.ElemCount,
        1u,
        index_offset + cmd.IdxOffset,
        static_cast<int32_t>(vertex_offset + cmd.VtxOffset),
        0);
    }
    index_offset += command_list->IdxBuffer.Size;
    vertex_offset += command_list->VtxBuffer.Size;
  }
}

auto
ImGuiRenderer::update_font(const std::string_view ttf_path,
                           const float font_size_pixels) -> void
{
  auto& io = ImGui::GetIO();
  ImFontConfig cfg{};
  cfg.FontDataOwnedByAtlas = false;
  cfg.RasterizerMultiply = 1.5f;
  cfg.SizePixels = std::ceil(font_size_pixels);
  cfg.PixelSnapH = true;
  cfg.OversampleH = 4;
  cfg.OversampleV = 4;
  ImFont* font = nullptr;
  if (!ttf_path.empty()) {
    font = io.Fonts->AddFontFromFileTTF(ttf_path.data(), cfg.SizePixels, &cfg);
  }
  io.Fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;

  unsigned char* pixels;
  int width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  TextureDescription description;
  description.pixel_data = std::as_bytes(
    std::span{ pixels, static_cast<std::size_t>(width * height * 4) });
  description.format = Format::RGBA_UN8;
  description.dimensions = {
    .width = static_cast<std::uint32_t>(width),
    .height = static_cast<std::uint32_t>(height),
    .depth = 1,
  };
  description.usage_bits = TextureUsageBits::Sampled;
  description.debug_name = "ImGui_Font_Texture";

  font_texture = VulkanTextureND::create(*context, description);
  io.Fonts->TexID = font_texture.index();
  io.FontDefault = font;
}

}