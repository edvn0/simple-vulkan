#pragma once

#include "sv/common.hpp"
#include "sv/object_holder.hpp"
#include <algorithm>
#include <vulkan/vulkan.h>

namespace sv {

class CommandBuffer;
class VulkanContext;

namespace detail {
template<typename Derived, typename DescriptionType>
class VkPipelineBase
{
protected:
  VkPipeline pipeline{ VK_NULL_HANDLE };
  VkPipelineLayout layout{ VK_NULL_HANDLE };
  VkShaderStageFlags stage_flags{};
  bool new_shader{ false };
  DescriptionType description{};

  VkDescriptorSetLayout descriptor_set_layout{ VK_NULL_HANDLE };
  VkDescriptorSetLayout last_descriptor_set_layout{ VK_NULL_HANDLE };

  std::unique_ptr<std::byte[]> specialisation_constants_storage{ nullptr };

public:
  [[nodiscard]] auto get_layout() const -> VkPipelineLayout { return layout; }
  [[nodiscard]] auto get_stage_flags() const -> VkShaderStageFlags
  {
    return stage_flags;
  }
  [[nodiscard]] auto get_pipeline() const -> VkPipeline { return pipeline; }

  auto update_shader(ShaderModuleHandle shader)
  {
    description.shader = shader;
    new_shader = true;
  }

  // Let derived classes implement create()
};
}

struct ComputePipelineDescription
{
  ShaderModuleHandle shader;
  SpecialisationConstantDescription specialisation_constants{};
  std::string entry_point{ "main" };
  std::string debug_name{};
};

class VulkanComputePipeline
  : public detail::VkPipelineBase<VulkanComputePipeline,
                                  ComputePipelineDescription>
{
  friend class ::sv::CommandBuffer;
  friend class ::sv::VulkanContext;

public:
  VulkanComputePipeline() { stage_flags = VK_SHADER_STAGE_COMPUTE_BIT; }

  static auto create(IContext* context, const ComputePipelineDescription& desc)
    -> Holder<ComputePipelineHandle>;
};

struct GraphicsPipelineDescription
{
  Topology topology{ Topology::Triangle };
  VertexInput vertex_input{};
  ShaderModuleHandle shader;
  SpecialisationConstantDescription specialisation_constants{};
  std::array<ColourAttachment, max_colour_attachments> color{};
  Format depth_format = Format::Invalid;
  Format stencil_format = Format::Invalid;
  CullMode cull_mode = CullMode::None;
  WindingMode winding = WindingMode::CCW;
  PolygonMode polygon_mode = PolygonMode::Fill;
  StencilState back_face_stencil = {};
  StencilState front_face_stencil = {};
  std::uint32_t sample_count = 1u;
  std::uint32_t patch_control_points = 0;
  float min_sample_shading = 0.0f;
  std::string debug_name{};

  [[nodiscard]] auto get_colour_attachments_count() const -> std::uint32_t
  {
    const auto result =
      std::ranges::count_if(color, [](const ColourAttachment& attachment) {
        return attachment.format != Format::Invalid;
      });
    return static_cast<std::uint32_t>(result);
  }

  auto is_compatible(const GraphicsPipelineDescription& other) const
  {
    return other.vertex_input == vertex_input;
  }
};

class VulkanGraphicsPipeline
  : public detail::VkPipelineBase<VulkanGraphicsPipeline,
                                  GraphicsPipelineDescription>
{
  friend class ::sv::CommandBuffer;
  friend class ::sv::VulkanContext;

  std::array<VkVertexInputBindingDescription,
             VertexInput::input_bindings_max_count>
    bindings{};
  std::array<VkVertexInputAttributeDescription,
             VertexInput::vertex_attribute_max_count>
    attributes{};
  std::uint32_t binding_count{ 0 };
  std::uint32_t attribute_count{ 0 };
  std::uint32_t view_mask{ 0 };

public:
  VulkanGraphicsPipeline() { stage_flags = VK_SHADER_STAGE_ALL_GRAPHICS; }

  [[nodiscard]] auto get_stage_flags() const -> VkShaderStageFlags
  {
    return stage_flags;
  }

  static auto create(IContext* context, const GraphicsPipelineDescription& desc)
    -> Holder<GraphicsPipelineHandle>;
};
}