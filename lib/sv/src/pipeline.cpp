#include "sv/pipeline.hpp"

#include "sv/context.hpp"

#include <bitset>
#include <cassert>

namespace sv {

namespace {

VkFormat
vertex_format_to_vk_format(VertexFormat fmt)
{
  switch (fmt) {
    case VertexFormat::Float1:
      return VK_FORMAT_R32_SFLOAT;
    case VertexFormat::Float2:
      return VK_FORMAT_R32G32_SFLOAT;
    case VertexFormat::Float3:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case VertexFormat::Float4:
      return VK_FORMAT_R32G32B32A32_SFLOAT;

    case VertexFormat::Byte1:
      return VK_FORMAT_R8_SINT;
    case VertexFormat::Byte2:
      return VK_FORMAT_R8G8_SINT;
    case VertexFormat::Byte3:
      return VK_FORMAT_R8G8B8_SINT;
    case VertexFormat::Byte4:
      return VK_FORMAT_R8G8B8A8_SINT;

    case VertexFormat::UByte1:
      return VK_FORMAT_R8_UINT;
    case VertexFormat::UByte2:
      return VK_FORMAT_R8G8_UINT;
    case VertexFormat::UByte3:
      return VK_FORMAT_R8G8B8_UINT;
    case VertexFormat::UByte4:
      return VK_FORMAT_R8G8B8A8_UINT;

    case VertexFormat::Short1:
      return VK_FORMAT_R16_SINT;
    case VertexFormat::Short2:
      return VK_FORMAT_R16G16_SINT;
    case VertexFormat::Short3:
      return VK_FORMAT_R16G16B16_SINT;
    case VertexFormat::Short4:
      return VK_FORMAT_R16G16B16A16_SINT;

    case VertexFormat::UShort1:
      return VK_FORMAT_R16_UINT;
    case VertexFormat::UShort2:
      return VK_FORMAT_R16G16_UINT;
    case VertexFormat::UShort3:
      return VK_FORMAT_R16G16B16_UINT;
    case VertexFormat::UShort4:
      return VK_FORMAT_R16G16B16A16_UINT;

    case VertexFormat::Byte2Norm:
      return VK_FORMAT_R8G8_SNORM;
    case VertexFormat::Byte4Norm:
      return VK_FORMAT_R8G8B8A8_SNORM;

    case VertexFormat::UByte2Norm:
      return VK_FORMAT_R8G8_UNORM;
    case VertexFormat::UByte4Norm:
      return VK_FORMAT_R8G8B8A8_UNORM;

    case VertexFormat::Short2Norm:
      return VK_FORMAT_R16G16_SNORM;
    case VertexFormat::Short4Norm:
      return VK_FORMAT_R16G16B16A16_SNORM;

    case VertexFormat::UShort2Norm:
      return VK_FORMAT_R16G16_UNORM;
    case VertexFormat::UShort4Norm:
      return VK_FORMAT_R16G16B16A16_UNORM;

    case VertexFormat::Int1:
      return VK_FORMAT_R32_SINT;
    case VertexFormat::Int2:
      return VK_FORMAT_R32G32_SINT;
    case VertexFormat::Int3:
      return VK_FORMAT_R32G32B32_SINT;
    case VertexFormat::Int4:
      return VK_FORMAT_R32G32B32A32_SINT;

    case VertexFormat::UInt1:
      return VK_FORMAT_R32_UINT;
    case VertexFormat::UInt2:
      return VK_FORMAT_R32G32_UINT;
    case VertexFormat::UInt3:
      return VK_FORMAT_R32G32B32_UINT;
    case VertexFormat::UInt4:
      return VK_FORMAT_R32G32B32A32_UINT;

    case VertexFormat::HalfFloat1:
      return VK_FORMAT_R16_SFLOAT;
    case VertexFormat::HalfFloat2:
      return VK_FORMAT_R16G16_SFLOAT;
    case VertexFormat::HalfFloat3:
      return VK_FORMAT_R16G16B16_SFLOAT;
    case VertexFormat::HalfFloat4:
      return VK_FORMAT_R16G16B16A16_SFLOAT;

    case VertexFormat::Int_2_10_10_10_REV:
      return VK_FORMAT_A2B10G10R10_SNORM_PACK32;

    case VertexFormat::Invalid:
    default:
      return VK_FORMAT_UNDEFINED;
  }
}
}

auto
VulkanComputePipeline::create(IContext& context,
                              const ComputePipelineDescription& desc)
  -> Holder<ComputePipelineHandle>
{
  VulkanComputePipeline cps{};
  cps.description = desc;
  cps.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;

  if (!desc.specialisation_constants.data.empty()) {
    cps.specialisation_constants_storage = std::make_unique<std::byte[]>(
      desc.specialisation_constants.data.size_bytes());
    std::memcpy(cps.specialisation_constants_storage.get(),
                desc.specialisation_constants.data.data(),
                desc.specialisation_constants.data.size_bytes());
    cps.description.specialisation_constants.data =
      std::span<std::byte>(cps.specialisation_constants_storage.get(),
                           desc.specialisation_constants.data.size_bytes());
  }

  return Holder{ &context,
                 context.get_compute_pipeline_pool().insert(std::move(cps)) };
}

auto
VulkanGraphicsPipeline::create(IContext& context,
                               const GraphicsPipelineDescription& desc)
  -> Holder<GraphicsPipelineHandle>
{
  const auto has_colour_attachments = desc.get_colour_attachments_count() != 0;
  const auto has_depth_attachment = desc.depth_format != Format::Invalid;
  const auto has_any_attachments =
    has_colour_attachments || has_depth_attachment;

  if (!has_any_attachments) {
    return Holder<GraphicsPipelineHandle>::invalid();
  }

  assert(desc.shader.valid());
  assert(!desc.debug_name.empty());

  VulkanGraphicsPipeline pipeline{};
  pipeline.description = desc;

  const auto& vertex_input = desc.vertex_input;
  std::bitset<VertexInput::input_bindings_max_count> used_bindings{};
  pipeline.attribute_count = vertex_input.get_attributes_count();
  for (auto i = 0U; i < pipeline.attribute_count; ++i) {
    const auto& [location, binding, format, offset] =
      vertex_input.attributes[i];
    assert(format != VertexFormat::Invalid);

    pipeline.attributes.at(i) = VkVertexInputAttributeDescription{
      .location = location,
      .binding = binding,
      .format = vertex_format_to_vk_format(format),
      .offset = static_cast<std::uint32_t>(offset),
    };

    if (!used_bindings.test(binding)) {
      used_bindings.set(binding);
      pipeline.bindings.at(pipeline.binding_count) =
        VkVertexInputBindingDescription{
          .binding = binding,
          .stride = vertex_input.input_bindings[binding].stride,
          .inputRate = vertex_input.input_bindings[binding].rate ==
                           VertexInput::VertexInputBinding::Rate::Vertex
                         ? VK_VERTEX_INPUT_RATE_VERTEX
                         : VK_VERTEX_INPUT_RATE_INSTANCE,
        };
      ++pipeline.binding_count;
    }
  }

  if (!desc.specialisation_constants.data.empty()) {
    pipeline.specialisation_constants_storage =
      std::make_unique<std::byte[]>(desc.specialisation_constants.data.size());
    std::memcpy(pipeline.specialisation_constants_storage.get(),
                desc.specialisation_constants.data.data(),
                desc.specialisation_constants.data.size());
    pipeline.description.specialisation_constants.data =
      std::span<std::byte>(pipeline.specialisation_constants_storage.get(),
                           desc.specialisation_constants.data.size());
  }

  pipeline.stage_flags =
    context.get_shader_module_pool().get(desc.shader)->get_shader_stage_flags();

  return Holder{
    &context,
    context.get_graphics_pipeline_pool().insert(std::move(pipeline)),
  };
}

}
