#pragma once

#include "sv/expected.hpp"

#include <cstdint>
#include <expected>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sv {

enum class ParseError : std::uint8_t
{
  invalid_pragma_syntax,
  unknown_shader_stage,
  duplicate_stage_entry,
  missing_stage_content,
  invalid_compute_entry_name
};
enum class ShaderStage : std::uint8_t
{
  vertex,
  fragment,
  geometry,
  tessellation_control,
  tessellation_evaluation,
  compute,
  task,
  mesh
};
auto
to_string(ShaderStage stage) -> std::string;
auto
parse_shader_stage(std::string_view stage_str)
  -> Expected<ShaderStage, ParseError>;

struct ShaderEntry
{
  ShaderStage stage;
  std::string entry_name{ "main" };
  std::string source_code;
  std::size_t line_number{ 0 };
};

struct ParsedShader
{
  std::vector<ShaderEntry> entries;
  std::unordered_map<std::string, size_t> stage_lookup;
};

class ShaderParser
{
  struct PragmaInfo
  {
    ShaderStage stage;
    std::string entry_name{ "main" };
    std::size_t line_number;
  };

  static auto parse_pragma_line(std::string_view line, size_t line_number)
    -> Expected<PragmaInfo, ParseError>
  {
    line = trim(line);

    if (!line.starts_with("#pragma") && !line.starts_with("# pragma")) {
      return unexpected<ParseError>(ParseError::invalid_pragma_syntax);
    }

    // Find "stage" keyword
    size_t stage_pos = line.find("stage");
    if (stage_pos == std::string_view::npos) {
      return unexpected<ParseError>(ParseError::invalid_pragma_syntax);
    }

    // Find the colon
    size_t colon_pos = line.find(':', stage_pos);
    if (colon_pos == std::string_view::npos) {
      return unexpected<ParseError>(ParseError::invalid_pragma_syntax);
    }

    // Extract everything after the colon
    std::string_view remainder = trim(line.substr(colon_pos + 1));

    // Check if it's a compute shader with entry point
    if (remainder.starts_with("compute")) {
      std::string entry_name;
      std::string_view compute_part = remainder.substr(7); // Skip "compute"
      compute_part = trim(compute_part);

      if (compute_part.starts_with("(")) {
        // Parse entry point name: compute("entry_name")
        size_t quote_start = compute_part.find('"');
        if (quote_start == std::string_view::npos) {
          return unexpected<ParseError>(ParseError::invalid_compute_entry_name);
        }

        size_t quote_end = compute_part.find('"', quote_start + 1);
        if (quote_end == std::string_view::npos) {
          return unexpected<ParseError>(ParseError::invalid_compute_entry_name);
        }

        entry_name = std::string(
          compute_part.substr(quote_start + 1, quote_end - quote_start - 1));
      } else if (!compute_part.empty()) {
        return unexpected<ParseError>(ParseError::invalid_pragma_syntax);
      }

      return PragmaInfo{ ShaderStage::compute, entry_name, line_number };
    }

    // For non-compute shaders, just parse the stage name
    auto stage_result = parse_shader_stage(remainder);
    if (!stage_result) {
      return unexpected<ParseError>(stage_result.error());
    }

    return PragmaInfo{ *stage_result, "", line_number };
  }

  static auto trim(std::string_view str) -> std::string_view
  {
    str.remove_prefix(std::min(str.find_first_not_of(" \t\r\n"), str.size()));
    str.remove_suffix(
      std::min(str.size() - str.find_last_not_of(" \t\r\n") - 1, str.size()));
    return str;
  }

  static auto create_stage_key(ShaderStage stage, const std::string& entry_name)
  {
    std::string key = to_string(stage);
    if (!entry_name.empty()) {
      key += "_" + entry_name;
    }
    return key;
  }

public:
  static auto parse(std::string_view shader_source)
    -> Expected<ParsedShader, ParseError>
  {
    ParsedShader result;
    std::istringstream stream{ shader_source.data() };
    std::string line;
    size_t line_number = 1;

    std::vector<PragmaInfo> pragmas;

    while (std::getline(stream, line)) {
      std::string_view line_view = trim(line);

      if (line_view.starts_with("#pragma stage") ||
          line_view.starts_with("# pragma stage")) {
        auto pragma_result = parse_pragma_line(line_view, line_number);
        if (!pragma_result) {
          return unexpected<ParseError>(pragma_result.error());
        }
        pragmas.push_back(*pragma_result);
      }
      line_number++;
    }

    if (pragmas.empty()) {
      return unexpected<ParseError>(ParseError::missing_stage_content);
    }

    // Second pass: extract shader code for each stage
    std::istringstream stream2(shader_source.data());
    line_number = 1;
    std::string current_content;
    size_t current_pragma_idx = 0;
    bool in_stage = false;

    while (std::getline(stream2, line)) {
      std::string_view line_view = trim(line);

      // Check if this is a pragma line
      bool is_pragma = (line_view.starts_with("#pragma stage") ||
                        line_view.starts_with("# pragma stage"));

      if (is_pragma) {
        // Save previous stage content if we were in one
        if (in_stage && current_pragma_idx > 0) {
          const auto& prev_pragma = pragmas[current_pragma_idx - 1];
          std::string stage_key =
            create_stage_key(prev_pragma.stage, prev_pragma.entry_name);

          if (result.stage_lookup.contains(stage_key)) {
            return unexpected<ParseError>(ParseError::duplicate_stage_entry);
          }

          result.entries.emplace_back(prev_pragma.stage,
                                      prev_pragma.entry_name,
                                      trim_string(current_content),
                                      prev_pragma.line_number);
          result.stage_lookup[stage_key] = result.entries.size() - 1;
        }

        // Start new stage
        current_content.clear();
        in_stage = true;
        current_pragma_idx++;
      } else if (in_stage) {
        // Accumulate content for current stage
        current_content += line + "\n";
      }

      line_number++;
    }

    if (in_stage && current_pragma_idx > 0) {
      const auto& last_pragma = pragmas[current_pragma_idx - 1];
      std::string stage_key =
        create_stage_key(last_pragma.stage, last_pragma.entry_name);

      if (result.stage_lookup.contains(stage_key)) {
        return unexpected<ParseError>(ParseError::duplicate_stage_entry);
      }

      result.entries.emplace_back(last_pragma.stage,
                                  last_pragma.entry_name,
                                  trim_string(current_content),
                                  last_pragma.line_number);
      result.stage_lookup[stage_key] = result.entries.size() - 1;
    }

    return result;
  }

  static auto prepend_preamble(ParsedShader&) -> bool;
  static auto destroy_context() -> void;

private:
  static auto trim_string(const std::string& str) -> std::string
  {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
      return "";

    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
  }
};

// Utility functions for working with parsed shaders
namespace ShaderUtils {
auto
find_stage(const ParsedShader& parsed,
           ShaderStage stage,
           const std::string& entry_name = "")
  -> Expected<const ShaderEntry*, ParseError>;

auto
find_all_compute_stages(const ParsedShader& parsed)
  -> std::vector<const ShaderEntry*>;

inline auto
error_to_string(ParseError error) -> std::string_view
{
  switch (error) {
    case ParseError::invalid_pragma_syntax:
      return "Invalid pragma syntax";
    case ParseError::unknown_shader_stage:
      return "Unknown shader stage";
    case ParseError::duplicate_stage_entry:
      return "Duplicate stage entry";
    case ParseError::missing_stage_content:
      return "Missing stage content";
    case ParseError::invalid_compute_entry_name:
      return "Invalid compute entry name";
  }
  return "Unknown error";
}
}

}
