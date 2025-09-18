#include "sv/shader/compilation.hpp"

#include "sv/expected.hpp"

#include "./compilation_impl.inl"

#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Include/glslang_c_shader_types.h>
#include <iostream>

namespace sv {

auto
parse_shader_stage(std::string_view stage_str)
  -> Expected<ShaderStage, ParseError>
{
  if (stage_str == "vertex")
    return ShaderStage::vertex;
  if (stage_str == "fragment")
    return ShaderStage::fragment;
  if (stage_str == "geometry")
    return ShaderStage::geometry;
  if (stage_str == "tessellation_control")
    return ShaderStage::tessellation_control;
  if (stage_str == "tessellation_evaluation")
    return ShaderStage::tessellation_evaluation;
  if (stage_str == "compute")
    return ShaderStage::compute;
  if (stage_str == "task")
    return ShaderStage::task;
  if (stage_str == "mesh")
    return ShaderStage::mesh;

  return unexpected<ParseError>(ParseError::unknown_shader_stage);
}

auto
to_string(ShaderStage stage) -> std::string
{
  switch (stage) {
    case ShaderStage::vertex:
      return "vertex";
    case ShaderStage::fragment:
      return "fragment";
    case ShaderStage::geometry:
      return "geometry";
    case ShaderStage::tessellation_control:
      return "tessellation_control";
    case ShaderStage::tessellation_evaluation:
      return "tessellation_evaluation";
    case ShaderStage::compute:
      return "compute";
    case ShaderStage::task:
      return "task";
    case ShaderStage::mesh:
      return "mesh";
  }
  return "unknown";
}

auto
ShaderParser::destroy_context() -> void
{
  glslang_finalize_process();
}

auto
ShaderParser::prepend_preamble(ParsedShader& parsed) -> bool
{
  static const std::string version_line = "#version 460";

  auto assert_does_not_have_version_or_extensions = [](const std::string& src) {
    if (src.find("#version") != std::string::npos) {
      std::cerr << "Shader source already contains a #version directive.\n";
      return false;
    }
    if (src.find("#extension") != std::string::npos) {
      std::cerr << "Shader source already contains an #extension directive.\n";
      return false;
    }
    return true;
  };

  for (auto& entry : parsed.entries) {
    if (!assert_does_not_have_version_or_extensions(entry.source_code)) {
      return false;
    }

    auto& src = entry.source_code;

    auto type = entry.stage;

    if (type == ShaderStage::task || type == ShaderStage::mesh) {
      constexpr auto append =
        R"(
      #version 460
      #extension GL_GOOGLE_include_directive : require
      #extension GL_EXT_buffer_reference : require
      #extension GL_EXT_buffer_reference_uvec2 : require
      #extension GL_EXT_debug_printf : enable
      #extension GL_EXT_nonuniform_qualifier : require
      #extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
      #extension GL_EXT_mesh_shader : require

)";
      src = append + src;
    } else if (type == ShaderStage::compute || type == ShaderStage::vertex ||
               type == ShaderStage::tessellation_control ||
               type == ShaderStage::tessellation_evaluation ||
               type == ShaderStage::geometry) {
      constexpr auto append =
        R"(
      #version 460
      #extension GL_GOOGLE_include_directive : require
      #extension GL_EXT_buffer_reference : require
      #extension GL_EXT_buffer_reference_uvec2 : require
      #extension GL_EXT_debug_printf : enable
      #extension GL_EXT_nonuniform_qualifier : require
      #extension GL_EXT_samplerless_texture_functions : require
      #extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
)";
      src = append + src;
    } else if (type == ShaderStage::fragment) {
      constexpr auto append =
        R"(
      #version 460
      #extension GL_GOOGLE_include_directive : require
      #extension GL_EXT_buffer_reference_uvec2 : require
      #extension GL_EXT_debug_printf : enable
      #extension GL_EXT_nonuniform_qualifier : require
      #extension GL_EXT_samplerless_texture_functions : require
      #extension GL_EXT_shader_explicit_arithmetic_types_float16 : require

      layout (set = 0, binding = 0) uniform texture2D textures[];
      layout (set = 0, binding = 2) uniform sampler   samplers[];

      vec4 textureBindless2D(uint textureid, uint samplerid, vec2 uv) {
        return texture(nonuniformEXT(sampler2D(textures[textureid], samplers[samplerid])), uv);
      }
      vec4 textureBindless2DLod(uint textureid, uint samplerid, vec2 uv, float lod) {
        return textureLod(nonuniformEXT(sampler2D(textures[textureid], samplers[samplerid])), uv, lod);
      }
      float textureBindless2DShadow(uint textureid, uint samplerid, vec3 uvw) {
        return texture(nonuniformEXT(sampler2DShadow(textures[textureid], samplers[samplerid])), uvw);
      }
      ivec2 textureBindlessSize2D(uint textureid) {
        return textureSize(nonuniformEXT(textures[textureid]), 0);
      }
      int textureBindlessQueryLevels2D(uint textureid) {
        return textureQueryLevels(nonuniformEXT(textures[textureid]));
      }
      int textureBindlessQueryLevelsCube(uint textureid) {
        return textureQueryLevels(nonuniformEXT(textures[textureid]));
      }
)";

      src = append + src;
    }
  }
  return true;
}

namespace ShaderUtils {
auto
find_stage(const ParsedShader& parsed,
           ShaderStage stage,
           const std::string& entry_name)
  -> Expected<const ShaderEntry*, ParseError>
{
  std::string key = (stage == ShaderStage::compute && !entry_name.empty())
                      ? "compute_" + entry_name
                      : to_string(stage);

  auto it = parsed.stage_lookup.find(key);
  if (it == parsed.stage_lookup.end()) {
    return unexpected<ParseError>(ParseError::missing_stage_content);
  }

  return &parsed.entries[it->second];
}

auto
find_all_compute_stages(const ParsedShader& parsed)
  -> std::vector<const ShaderEntry*>
{
  std::vector<const ShaderEntry*> compute_entries;

  for (const ShaderEntry& entry : parsed.entries) {
    if (entry.stage == ShaderStage::compute) {
      compute_entries.push_back(&entry);
    }
  }

  return compute_entries;
}
}
}
