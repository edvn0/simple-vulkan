#pragma once

#include "sv/expected.hpp"
#include "sv/scope_exit.hpp"

#include <filesystem>
#include <fstream>
#include <glslang/Include/glslang_c_interface.h>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

namespace {
// Context structure to manage include result lifetimes
struct IncludeContext
{
  std::unordered_map<void*, std::unique_ptr<glsl_include_result_t>> results;
  std::unordered_map<void*, std::string> header_names;
  std::unordered_map<void*, std::string> contents;
};

auto
read_file_to_string(const std::filesystem::path& file_path) -> std::string
{
  std::ifstream file(file_path, std::ios::binary);
  if (!file) {
    return {};
  }

  std::ostringstream contents;
  contents << file.rdbuf();
  return contents.str();
}

// Callback for system includes: #include <file.glsl>
auto
include_system_callback(void* ctx, const char* header_name, const char*, size_t)
  -> glsl_include_result_t*
{
  auto* context = static_cast<IncludeContext*>(ctx);

  // Resolve system includes from assets/shaders/include/
  std::filesystem::path include_path = "assets/shaders/include";
  std::filesystem::path full_path = include_path / header_name;

  std::string content = read_file_to_string(full_path);
  if (content.empty()) {
    return nullptr; // File not found or empty
  }

  auto result = std::make_unique<glsl_include_result_t>();
  std::string header_name_str(header_name);

  void* result_ptr = result.get();
  context->header_names[result_ptr] = std::move(header_name_str);
  context->contents[result_ptr] = std::move(content);

  result->header_name = context->header_names[result_ptr].c_str();
  result->header_data = context->contents[result_ptr].c_str();
  result->header_length = context->contents[result_ptr].length();

  glsl_include_result_t* result_raw = result.release();
  context->results[result_raw] =
    std::unique_ptr<glsl_include_result_t>(result_raw);

  return result_raw;
}

auto
include_local_callback(void* ctx,
                       const char* header_name,
                       const char* includer_name,
                       size_t include_depth) -> glsl_include_result_t*
{
  // You could modify this to be relative to the includer_name if needed
  return include_system_callback(
    ctx, header_name, includer_name, include_depth);
}

// Callback to free include result
auto
free_include_result_callback(void* ctx, glsl_include_result_t* result)
{
  if (!result || !ctx) {
    return 0;
  }

  auto* context = static_cast<IncludeContext*>(ctx);

  // Clean up the stored strings and result
  context->header_names.erase(result);
  context->contents.erase(result);
  context->results.erase(result);

  return 1; // Success
}

auto
compile_shader(glslang_stage_t stage,
               const std::string& source_code,
               std::vector<std::uint8_t>& output,
               const glslang_resource_t* resources)
  -> sv::Expected<void, std::string>
{
  IncludeContext include_ctx;

  // Setup include callbacks
  static glsl_include_callbacks_t include_callbacks = {
    .include_system = include_system_callback,
    .include_local = include_local_callback,
    .free_include_result = free_include_result_callback
  };

  const glslang_input_t input = {
    .language = GLSLANG_SOURCE_GLSL,
    .stage = stage,
    .client = GLSLANG_CLIENT_VULKAN,
    .client_version = GLSLANG_TARGET_VULKAN_1_4,
    .target_language = GLSLANG_TARGET_SPV,
    .target_language_version = GLSLANG_TARGET_SPV_1_6,
    .code = source_code.c_str(),
    .default_version = 100,
    .default_profile = GLSLANG_NO_PROFILE,
    .force_default_version_and_profile = false,
    .forward_compatible = false,
    .messages = GLSLANG_MSG_DEFAULT_BIT,
    .resource = resources,
    .callbacks = include_callbacks,
    .callbacks_ctx = &include_ctx, // Pass our context
  };

  glslang_shader_t* shader = glslang_shader_create(&input);
  SCOPE_EXIT
  {
    glslang_shader_delete(shader);
  };

  if (!glslang_shader_preprocess(shader, &input)) {
    std::string error = glslang_shader_get_info_log(shader);
    auto debug_log = glslang_shader_get_info_debug_log(shader);
    std::cout << "Preprocessing error: " << error << std::endl;
    if (const std::string_view log{ debug_log }; !log.empty()) {
      std::cout << "Debug log: " << debug_log << std::endl;
    }
    return sv::unexpected<std::string>(error);
  }

  if (!glslang_shader_parse(shader, &input)) {
    std::string error = glslang_shader_get_info_log(shader);
    auto debug_log = glslang_shader_get_info_debug_log(shader);
    std::cout << "Parsing error: " << error << std::endl;
    if (const std::string_view log{ debug_log }; !log.empty()) {
      std::cout << "Debug log: " << debug_log << std::endl;
    }
    return sv::unexpected<std::string>(error);
  }

  glslang_program_t* program = glslang_program_create();
  glslang_program_add_shader(program, shader);
  SCOPE_EXIT
  {
    glslang_program_delete(program);
  };
  if (!glslang_program_link(
        program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)) {
    std::string error = glslang_program_get_info_log(program);
    auto debug_log = glslang_program_get_info_debug_log(program);
    std::cout << "Linking error: " << error << std::endl;
    if (const std::string_view log{ debug_log }; !log.empty()) {
      std::cout << "Debug log: " << debug_log << std::endl;
    }
    return sv::unexpected<std::string>(error);
  }

  glslang_spv_options_t options = {
    .generate_debug_info = true,
    .strip_debug_info = false,
    .disable_optimizer = false,
    .optimize_size = false,
    .disassemble = false,
    .validate = true,
    .emit_nonsemantic_shader_debug_info = false,
    .emit_nonsemantic_shader_debug_source = false,
    .compile_only = false,
    .optimize_allow_expanded_id_bound = false,
  };
  glslang_program_SPIRV_generate_with_options(program, input.stage, &options);

  if (glslang_program_SPIRV_get_messages(program) != nullptr) {
    std::string error = glslang_program_SPIRV_get_messages(program);
    std::cout << "SPIR-V generation error: " << error << std::endl;
  }

  const auto* spirv =
    std::bit_cast<const std::uint8_t*>(glslang_program_SPIRV_get_ptr(program));
  const auto byte_count =
    glslang_program_SPIRV_get_size(program) * sizeof(uint32_t);

  output.resize(byte_count);
  std::copy(spirv, spirv + byte_count, output.begin());
  return {};
}

}
