#pragma once

#include <bit>
#include <compare>
#include <cstdint>

namespace sv {

struct IContext;

static constexpr std::uint32_t invalid_generation = 0U;

template<typename>
class Handle final
{
  Handle(const std::uint32_t index, const std::uint32_t generation)
    : handle_index(index)
    , handle_generation(generation)
  {
  }

  std::uint32_t handle_index{ 0 };
  std::uint32_t handle_generation{ invalid_generation };

  template<typename T_, typename TImpl, bool LF>
  friend class Pool;

public:
  Handle() = default;

  [[nodiscard]] auto valid() const -> bool
  {
    return handle_generation != invalid_generation;
  }
  [[nodiscard]] auto empty() const -> bool { return !valid(); }
  explicit operator bool() const { return valid(); }

  [[nodiscard]] auto index() const -> std::uint32_t { return handle_index; }
  [[nodiscard]] auto generation() const -> std::uint32_t
  {
    return handle_generation;
  }

  template<typename V = void*>
  [[nodiscard]] auto explicit_cast() const -> V*
  {
    return std::bit_cast<V*>(static_cast<std::ptrdiff_t>(handle_index));
  }

  auto operator<=>(const Handle& other) const = default;
};
static_assert(sizeof(Handle<class K>) == sizeof(std::uint64_t));

using TextureHandle = Handle<class TextureND>;
using SamplerHandle = Handle<class Sampler>;
using BufferHandle = Handle<class DeviceBuffer>;
using GraphicsPipelineHandle = Handle<class GraphicsPipeline>;
using ShaderModuleHandle = Handle<class Shader>;
using ComputePipelineHandle = Handle<class ComputePipeline>;

}