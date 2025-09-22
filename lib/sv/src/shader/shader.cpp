#include "sv/shader/shader.hpp"

#include <expected>
#include <fstream>

#include "sv/context.hpp"
#include "sv/scope_exit.hpp"

#include "./compilation_impl.inl"

#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>
#include <iostream>

#include <spirv_reflect.h>

static constexpr glslang_resource_t
    default_resource = { .max_lights = 32,
                         .max_clip_planes = 6,
                         .max_texture_units = 32,
                         .max_texture_coords = 32,
                         .max_vertex_attribs = 64,
                         .max_vertex_uniform_components = 4096,
                         .max_varying_floats = 64,
                         .max_vertex_texture_image_units = 32,
                         .max_combined_texture_image_units = 80,
                         .max_texture_image_units = 32,
                         .max_fragment_uniform_components = 4096,
                         .max_draw_buffers = 32,
                         .max_vertex_uniform_vectors = 128,
                         .max_varying_vectors = 8,
                         .max_fragment_uniform_vectors = 16,
                         .max_vertex_output_vectors = 16,
                         .max_fragment_input_vectors = 15,
                         .min_program_texel_offset = -8,
                         .max_program_texel_offset = 7,
                         .max_clip_distances = 8,
                         .max_compute_work_group_count_x = 65535,
                         .max_compute_work_group_count_y = 65535,
                         .max_compute_work_group_count_z = 65535,
                         .max_compute_work_group_size_x = 1024,
                         .max_compute_work_group_size_y = 1024,
                         .max_compute_work_group_size_z = 64,
                         .max_compute_uniform_components = 1024,
                         .max_compute_texture_image_units = 16,
                         .max_compute_image_uniforms = 8,
                         .max_compute_atomic_counters = 8,
                         .max_compute_atomic_counter_buffers = 1,
                         .max_varying_components = 60,
                         .max_vertex_output_components = 64,
                         .max_geometry_input_components = 64,
                         .max_geometry_output_components = 128,
                         .max_fragment_input_components = 128,
                         .max_image_units = 8,
                         .max_combined_image_units_and_fragment_outputs = 8,
                         .max_combined_shader_output_resources = 8,
                         .max_image_samples = 0,
                         .max_vertex_image_uniforms = 0,
                         .max_tess_control_image_uniforms = 0,
                         .max_tess_evaluation_image_uniforms = 0,
                         .max_geometry_image_uniforms = 0,
                         .max_fragment_image_uniforms = 8,
                         .max_combined_image_uniforms = 8,
                         .max_geometry_texture_image_units = 16,
                         .max_geometry_output_vertices = 256,
                         .max_geometry_total_output_components = 1024,
                         .max_geometry_uniform_components = 1024,
                         .max_geometry_varying_components = 64,
                         .max_tess_control_input_components = 128,
                         .max_tess_control_output_components = 128,
                         .max_tess_control_texture_image_units = 16,
                         .max_tess_control_uniform_components = 1024,
                         .max_tess_control_total_output_components = 4096,
                         .max_tess_evaluation_input_components = 128,
                         .max_tess_evaluation_output_components = 128,
                         .max_tess_evaluation_texture_image_units = 16,
                         .max_tess_evaluation_uniform_components = 1024,
                         .max_tess_patch_components = 120,
                         .max_patch_vertices = 32,
                         .max_tess_gen_level = 64,
                         .max_viewports = 16,
                         .max_vertex_atomic_counters = 0,
                         .max_tess_control_atomic_counters = 0,
                         .max_tess_evaluation_atomic_counters = 0,
                         .max_geometry_atomic_counters = 0,
                         .max_fragment_atomic_counters = 8,
                         .max_combined_atomic_counters = 8,
                         .max_atomic_counter_bindings = 1,
                         .max_vertex_atomic_counter_buffers = 0,
                         .max_tess_control_atomic_counter_buffers = 0,
                         .max_tess_evaluation_atomic_counter_buffers = 0,
                         .max_geometry_atomic_counter_buffers = 0,
                         .max_fragment_atomic_counter_buffers = 1,
                         .max_combined_atomic_counter_buffers = 1,
                         .max_atomic_counter_buffer_size = 16384,
                         .max_transform_feedback_buffers = 4,
                         .max_transform_feedback_interleaved_components = 64,
                         .max_cull_distances = 8,
                         .max_combined_clip_and_cull_distances = 8,
                         .max_samples = 4,
                         .max_mesh_output_vertices_nv = 256,
                         .max_mesh_output_primitives_nv = 512,
                         .max_mesh_work_group_size_x_nv = 32,
                         .max_mesh_work_group_size_y_nv = 1,
                         .max_mesh_work_group_size_z_nv = 1,
                         .max_task_work_group_size_x_nv = 32,
                         .max_task_work_group_size_y_nv = 1,
                         .max_task_work_group_size_z_nv = 1,
                         .max_mesh_view_count_nv = 4,
                         .max_mesh_output_vertices_ext = 256,
                         .max_mesh_output_primitives_ext = 256,
                         .max_mesh_work_group_size_x_ext = 128,
                         .max_mesh_work_group_size_y_ext = 128,
                         .max_mesh_work_group_size_z_ext = 128,
                         .max_task_work_group_size_x_ext = 128,
                         .max_task_work_group_size_y_ext = 128,
                         .max_task_work_group_size_z_ext = 128,
                         .max_mesh_view_count_ext = 4,
                         .max_dual_source_draw_buffers_ext = 1,

                         .limits = {
                           .non_inductive_for_loops = true,
                           .while_loops = true,
                           .do_while_loops = true,
                           .general_uniform_indexing = true,
                           .general_attribute_matrix_vector_indexing = true,
                           .general_varying_indexing = true,
                           .general_sampler_indexing = true,
                           .general_variable_indexing = true,
                           .general_constant_matrix_vector_indexing = true,
                         }, };

namespace sv {

namespace {
auto
to_glslang_stage(const ShaderStage stage) -> glslang_stage_t
{
  switch (stage) {
    case ShaderStage::vertex:
      return GLSLANG_STAGE_VERTEX;
    case ShaderStage::fragment:
      return GLSLANG_STAGE_FRAGMENT;
    case ShaderStage::geometry:
      return GLSLANG_STAGE_GEOMETRY;
    case ShaderStage::tessellation_control:
      return GLSLANG_STAGE_TESSCONTROL;
    case ShaderStage::tessellation_evaluation:
      return GLSLANG_STAGE_TESSEVALUATION;
    case ShaderStage::compute:
      return GLSLANG_STAGE_COMPUTE;
    default:
      std::cerr << "Unknown shader stage: " << to_string(stage) << std::endl;
      return GLSLANG_STAGE_VERTEX; // fallback to vertex stage
  }
}
}

auto
VulkanShader::create(IContext& context, const std::filesystem::path& path)
  -> Holder<ShaderModuleHandle>
{
  auto& pool = context.get_shader_module_pool();
  auto compiled = VulkanShader::compile(context, path);
  if (!compiled) {
    std::cerr << std::format("File {} - ", path.filename().string())
              << compiled.error().error << "\n";
    return {};
  }

  const auto handle = pool.insert(std::move(compiled.value()));
  if (!handle.valid()) {
    return {};
  }

  return Holder{ &context, handle };
}

auto
VulkanShader::compile(IContext& context, const std::filesystem::path& path)
  -> Expected<VulkanShader, ShaderError>
{
  auto stream = std::ifstream{ path };
  if (!stream) {
    return unexpected<ShaderError>(ShaderError(
      ShaderError::Code::file_not_found,
      std::format("Failed to open shader file: {}", path.string())));
  }

  std::string source_code;
  try {
    source_code = std::string((std::istreambuf_iterator<char>(stream)),
                              std::istreambuf_iterator<char>());
  } catch (const std::exception& e) {
    return unexpected<ShaderError>(
      ShaderError(ShaderError::Code::file_read_failed,
                  std::format("Failed to read shader file: {}", e.what())));
  }

  auto parsed = ShaderParser::parse(source_code);
  if (!parsed) {
    return unexpected<ShaderError>(
      ShaderError(ShaderError::Code::parse_failed,
                  std::format("Failed to parse shader: {}",
                              ShaderUtils::error_to_string(parsed.error()))));
  }

  if (!ShaderParser::prepend_preamble(*parsed)) {
    return unexpected<ShaderError>(ShaderError(
      ShaderError::Code::preamble_failed, "Failed to prepend shader preamble"));
  }

  std::vector<VulkanShader::StageModule> modules;
  PushConstantInfo push_constant_info{};

  for (const auto& entry : parsed->entries) {
    std::vector<std::uint8_t> spirv;
    auto result = compile_shader(to_glslang_stage(entry.stage),
                                 entry.source_code,
                                 spirv,
                                 &default_resource);

    if (!result) {
      return unexpected<ShaderError>(
        ShaderError(ShaderError::Code::compilation_failed,
                    std::format("Compilation failed for stage {}: {}",
                                to_string(entry.stage),
                                result.error())));
    }

    const VkShaderModuleCreateInfo create_info{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .codeSize = spirv.size(),
      .pCode = reinterpret_cast<const std::uint32_t*>(spirv.data()),
    };

    VkShaderModule module;
    if (vkCreateShaderModule(
          context.get_device(), &create_info, nullptr, &module) != VK_SUCCESS) {
      return unexpected<ShaderError>(
        ShaderError(ShaderError::Code::module_creation_failed,
                    std::format("vkCreateShaderModule failed for stage {}",
                                to_string(entry.stage))));
    }

    // Reflection handling
    SpvReflectShaderModule reflect_module{};
    if (spvReflectCreateShaderModule(
          spirv.size(), spirv.data(), &reflect_module) ==
        SPV_REFLECT_RESULT_SUCCESS) {

      auto count = 0U;
      if (auto res = spvReflectEnumeratePushConstantBlocks(
            &reflect_module, &count, nullptr);
          res == SPV_REFLECT_RESULT_SUCCESS && count > 0) {

        std::vector<SpvReflectBlockVariable*> blocks(count);
        res = spvReflectEnumeratePushConstantBlocks(
          &reflect_module, &count, blocks.data());

        if (res == SPV_REFLECT_RESULT_SUCCESS) {
          for (auto* block : blocks) {
            if (block->size > push_constant_info.size) {
              push_constant_info.size = block->size;
            }
            push_constant_info.stages |= to_vk_stage(entry.stage);
          }
        }
      }
      spvReflectDestroyShaderModule(&reflect_module);
    }

    modules.emplace_back(entry.stage,
                         entry.entry_name.empty() ? "main" : entry.entry_name,
                         module);
  }

  std::uint32_t total_stages{};
  for (const auto& stage :
       parsed->entries |
         std::views::transform([](const auto& e) { return e.stage; })) {
    total_stages |= to_vk_stage(stage);
  }

  auto flags = static_cast<VkShaderStageFlagBits>(total_stages);
  push_constant_info.stages = flags;

  return VulkanShader(context, std::move(modules), push_constant_info, flags);
}

VulkanShader::VulkanShader(IContext& ctx,
                           std::vector<StageModule>&& mods,
                           const PushConstantInfo push_info,
                           const VkShaderStageFlagBits flag_bits)
  : push_constant_info(push_info)
  , context(dynamic_cast<VulkanContext*>(&ctx))
  , modules(std::move(mods))
  , flags(flag_bits)
{

  static bool glslang_is_initialised = false;
  if (!glslang_is_initialised) {
    glslang_initialize_process();
    glslang_is_initialised = true;
  }
}
}
