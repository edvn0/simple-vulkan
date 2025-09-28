#pragma once

#include "sv/object_handle.hpp"
#include "sv/object_holder.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <expected>
#include <format>
#include <glm/glm.hpp>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vulkan/vulkan.h>

#define BIT_FIELD(X)                                                           \
  constexpr auto operator|(X lhs, X rhs)->X                                    \
  {                                                                            \
    using C = std::underlying_type_t<X>;                                       \
    return static_cast<X>(static_cast<C>(lhs) | static_cast<C>(rhs));          \
  }                                                                            \
  constexpr auto operator&(X lhs, X rhs)->X                                    \
  {                                                                            \
    using C = std::underlying_type_t<X>;                                       \
    return static_cast<X>(static_cast<C>(lhs) & static_cast<C>(rhs));          \
  }                                                                            \
  constexpr auto operator==(X lhs, std::integral auto R)->bool                 \
  {                                                                            \
    return static_cast<bool>(static_cast<std::underlying_type_t<X>>(lhs) ==    \
                             R);                                               \
  }

static constexpr auto
bit(const auto x)
{
  return 1 << x;
}

namespace sv {

template<typename T>
inline auto
make_error(typename T::Code c, std::string_view message) -> std::unexpected<T>
{
  return std::unexpected(T{ c, std::string{ message } });
}

template<typename T>
inline auto
make_error(typename T::Code c, const char* message) -> std::unexpected<T>
{
  return std::unexpected(T{ c, std::string{ message } });
}

template<typename T>
inline auto
make_error(const T& err) -> std::unexpected<T>
{
  return std::unexpected(err);
}

static constexpr auto max_colour_attachments = 8U;

enum class IndexFormat : std::uint8_t
{
  UI16,
  UI32,
  UI8,
};

enum class Topology : std::uint8_t
{
  Point,
  Line,
  LineStrip,
  Triangle,
  TriangleStrip,
  Patch,
};

enum class CullMode : std::uint8_t
{
  None,
  Front,
  Back
};

enum class WindingMode : std::uint8_t
{
  CCW,
  CW
};

enum class ColorSpace : std::uint8_t
{
  SRGB_NONLINEAR,
  SRGB_EXTENDED_LINEAR,
  HDR10,
  BT709_LINEAR,
};

enum class TextureType : std::uint8_t
{
  Two,
  Three,
  Cube,
};

enum class Swizzle : std::uint8_t
{
  Default = 0,
  Zero,
  One,
  R,
  G,
  B,
  A,
};

enum class ImageTiling : std::uint8_t
{
  Optimal = 0,
  Linear = 1,
};

struct ComponentMapping
{
  Swizzle r = Swizzle::Default;
  Swizzle g = Swizzle::Default;
  Swizzle b = Swizzle::Default;
  Swizzle a = Swizzle::Default;
  constexpr auto identity() const
  {
    return r == Swizzle::Default && g == Swizzle::Default &&
           b == Swizzle::Default && a == Swizzle::Default;
  }
};

struct Dimensions
{
  std::uint32_t width = 1;
  std::uint32_t height = 1;
  std::uint32_t depth = 1;
  inline auto divide1D(std::integral auto v) const
  {
    return Dimensions{
      .width = width / v,
      .height = height,
      .depth = depth,
    };
  }
  inline auto divide2D(std::integral auto v) const
  {
    return Dimensions{
      .width = width / v,
      .height = height / v,
      .depth = depth,
    };
  }
  inline auto divide3D(std::integral auto v) const
  {
    return Dimensions{
      .width = width / v,
      .height = height / v,
      .depth = depth / v,
    };
  }
  inline auto operator<=>(const Dimensions& other) const = default;
};

enum class CompareOp : std::uint8_t
{
  Never = 0,
  Less,
  Equal,
  LessEqual,
  Greater,
  NotEqual,
  GreaterEqual,
  AlwaysPass
};

enum class StencilOp : std::uint8_t
{
  Keep = 0,
  Zero,
  Replace,
  IncrementClamp,
  DecrementClamp,
  Invert,
  IncrementWrap,
  DecrementWrap
};

enum class BlendOp : std::uint8_t
{
  Add = 0,
  Subtract,
  ReverseSubtract,
  Min,
  Max
};

enum class BlendFactor : std::uint8_t
{
  Zero = 0,
  One,
  SrcColor,
  OneMinusSrcColor,
  SrcAlpha,
  OneMinusSrcAlpha,
  DstColor,
  OneMinusDstColor,
  DstAlpha,
  OneMinusDstAlpha,
  SrcAlphaSaturated,
  BlendColor,
  OneMinusBlendColor,
  BlendAlpha,
  OneMinusBlendAlpha,
  Src1Color,
  OneMinusSrc1Color,
  Src1Alpha,
  OneMinusSrc1Alpha
};

enum class LoadOp : std::uint8_t
{
  Invalid = 0,
  DontCare,
  Load,
  Clear,
  None,
};

enum class StoreOp : std::uint8_t
{
  DontCare = 0,
  Store,
  MsaaResolve,
  None,
};

enum class ResolveMode : std::uint8_t
{
  None = 0,
  SampleZero, // always supported
  Average,
  Min,
  Max,
};

enum class PolygonMode : uint8_t
{
  Fill = 0,
  Line = 1,
};

enum class VertexFormat
{
  Invalid = 0,

  Float1,
  Float2,
  Float3,
  Float4,

  Byte1,
  Byte2,
  Byte3,
  Byte4,

  UByte1,
  UByte2,
  UByte3,
  UByte4,

  Short1,
  Short2,
  Short3,
  Short4,

  UShort1,
  UShort2,
  UShort3,
  UShort4,

  Byte2Norm,
  Byte4Norm,

  UByte2Norm,
  UByte4Norm,

  Short2Norm,
  Short4Norm,

  UShort2Norm,
  UShort4Norm,

  Int1,
  Int2,
  Int3,
  Int4,

  UInt1,
  UInt2,
  UInt3,
  UInt4,

  HalfFloat1,
  HalfFloat2,
  HalfFloat3,
  HalfFloat4,

  Int_2_10_10_10_REV,
};

enum class Format : uint8_t
{
  Invalid = 0,

  R_UN8,
  R_UI8,
  R_UI16,
  R_UI32,
  R_UN16,
  R_F16,
  R_F32,

  RG_UN8,
  RG_UI16,
  RG_UI32,
  RG_UN16,
  RG_F16,
  RG_F32,

  RGBA_UN8,
  RGBA_UI32,
  RGBA_UI16,
  RGBA_F16,
  RGBA_F32,
  RGBA_SRGB8,

  BGRA_UN8,
  BGRA_SRGB8,

  A2B10G10R10_UN,
  A2R10G10B10_UN,

  ETC2_RGB8,
  ETC2_SRGB8,
  BC7_RGBA,

  Z_UN16,
  Z_UN24,
  Z_F32,
  Z_UN24_S_UI8,
  Z_F32_S_UI8,

  YUV_NV12,
  YUV_420p,
};

struct VertexInput final
{
  static constexpr std::uint32_t vertex_attribute_max_count = 16;
  static constexpr std::uint32_t input_bindings_max_count = 16;
  struct VertexAttribute final
  {
    std::uint32_t location = 0;
    std::uint32_t binding = 0;
    VertexFormat format = VertexFormat::Invalid;
    std::uintptr_t offset = 0;

    auto operator<=>(const VertexAttribute&) const = default;
  };
  std::array<VertexAttribute, vertex_attribute_max_count> attributes{};
  struct VertexInputBinding final
  {
    std::uint32_t stride = 0;
    enum class Rate : std::uint8_t
    {
      Vertex = 0,
      Instance = 1,
    } rate = Rate::Vertex;

    auto operator<=>(const VertexInputBinding& other) const = default;
  };
  std::array<VertexInputBinding, input_bindings_max_count> input_bindings{};

  [[nodiscard]] auto get_attributes_count() const
  {
    std::uint32_t n = 0;
    while (n < vertex_attribute_max_count &&
           attributes[n].format != VertexFormat::Invalid) {
      n++;
    }
    return n;
  }

  [[nodiscard]] auto get_input_bindings_count() const
  {
    uint32_t n = 0;
    while (n < input_bindings_max_count && input_bindings[n].stride) {
      n++;
    }
    return n;
  }

  [[nodiscard]] auto compute_vertex_size() const
  {
    std::uint32_t vertex_size = 0;
    for (auto i = 0U; i < vertex_attribute_max_count &&
                      attributes[i].format != VertexFormat::Invalid;
         i++) {
      vertex_size +=
        get_vertex_format_size<std::uint32_t>(attributes[i].format);
    }
    return vertex_size;
  }

  auto operator<=>(const VertexInput& other) const = default;

  static auto create(
    const std::initializer_list<const VertexFormat> formats,
    const std::initializer_list<const VertexFormat> instance_formats = {})
    -> VertexInput
  {
    VertexInput vi{};
    std::uint32_t offset = 0;
    std::uint32_t location = 0;
    for (const auto format : formats) {
      vi.attributes.at(location) = VertexAttribute{
        .location = location,
        .binding = 0,
        .format = format,
        .offset = offset,
      };
      offset += vi.get_vertex_format_size<std::uint32_t>(format);
      location++;
    }
    vi.input_bindings.at(0) = VertexInputBinding{ .stride = offset };

    for (const auto format : instance_formats) {
      vi.attributes.at(location) = VertexAttribute{
        .location = location,
        .binding = 1,
        .format = format,
        .offset = offset,
      };
      offset += vi.get_vertex_format_size<std::uint32_t>(format);
      location++;
    }
    if (instance_formats.size() > 0) {
      vi.input_bindings[1] = VertexInputBinding{
        .stride = offset,
        .rate = VertexInputBinding::Rate::Instance,
      };
    }

    return vi;
  }

private:
  template<std::unsigned_integral Out = std::size_t>
  auto get_vertex_format_size(VertexFormat format) const -> Out
  {
#define SIZE4(LVKBaseType, BaseType)                                           \
  case VertexFormat::LVKBaseType##1:                                           \
    return static_cast<Out>(sizeof(BaseType) * 1ULL);                          \
  case VertexFormat::LVKBaseType##2:                                           \
    return static_cast<Out>(sizeof(BaseType) * 2ULL);                          \
  case VertexFormat::LVKBaseType##3:                                           \
    return static_cast<Out>(sizeof(BaseType) * 3ULL);                          \
  case VertexFormat::LVKBaseType##4:                                           \
    return static_cast<Out>(sizeof(BaseType) * 4ULL);
#define SIZE2_4_NORM(LVKBaseType, BaseType)                                    \
  case VertexFormat::LVKBaseType##2Norm:                                       \
    return static_cast<Out>(sizeof(BaseType) * 2ULL);                          \
  case VertexFormat::LVKBaseType##4Norm:                                       \
    return static_cast<Out>(sizeof(BaseType) * 4ULL);

    switch (format) {
      SIZE4(Float, float);
      SIZE4(Byte, uint8_t);
      SIZE4(UByte, uint8_t);
      SIZE4(Short, uint16_t);
      SIZE4(UShort, uint16_t);
      SIZE2_4_NORM(Byte, uint8_t);
      SIZE2_4_NORM(UByte, uint8_t);
      SIZE2_4_NORM(Short, uint16_t);
      SIZE2_4_NORM(UShort, uint16_t);
      SIZE4(Int, uint32_t);
      SIZE4(UInt, uint32_t);
      SIZE4(HalfFloat, uint16_t);
      case VertexFormat::Int_2_10_10_10_REV:
        return sizeof(uint32_t);
      default:
        assert(false);
        return 0;
    }
#undef SIZE4
#undef SIZE2_4_NORM
  }
};

struct ColourAttachment
{
  Format format = Format::Invalid;
  bool blend_enabled = false;
  BlendOp rgb_blend_op = BlendOp::Add;
  BlendOp alpha_blend_op = BlendOp::Add;
  BlendFactor src_rgb_blend_factor = BlendFactor::One;
  BlendFactor src_alpha_blend_factor = BlendFactor::One;
  BlendFactor dst_rgb_blend_factor = BlendFactor::Zero;
  BlendFactor dst_alpha_blend_factor = BlendFactor::Zero;
};

struct SpecialisationConstantDescription
{
  struct SpecialisationConstantEntry
  {
    std::uint32_t constant_id = 0;
    std::uint32_t offset = 0;
    std::size_t size = 0;
  };

  static constexpr auto max_specialization_constants = 16U;

  std::array<SpecialisationConstantEntry, max_specialization_constants>
    entries{};
  std::span<const std::byte> data{};

  auto get_specialisation_constants_count() const
  {
    std::uint32_t n = 0;
    while (n < max_specialization_constants && entries[n].size) {
      n++;
    }
    return n;
  }

  [[nodiscard]] auto at(std::unsigned_integral auto i) -> decltype(auto)
  {
    return entries.at(i);
  }
};

struct Dependencies
{
  static constexpr auto max_dependencies = 4U;
  std::array<TextureHandle, max_dependencies> textures = {};
  std::array<BufferHandle, max_dependencies> buffers = {};
};

using ClearColourValue = std::variant<std::array<float, 4>,
                                      std::array<std::uint32_t, 4>,
                                      std::array<std::int32_t, 4>>;

struct RenderPass final
{
  struct AttachmentDescription final
  {
    LoadOp load_op = LoadOp::Invalid;
    StoreOp store_op = StoreOp::Store;
    ResolveMode resolve_mode = ResolveMode::Average;
    std::uint8_t layer = 0;
    std::uint8_t level = 0;
    ClearColourValue clear_colour =
      std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f };
    float clear_depth = 1.0f;
    uint32_t clear_stencil = 0;
  };

  std::array<AttachmentDescription, max_colour_attachments> color{};
  AttachmentDescription depth = {
    .load_op = LoadOp::DontCare,
    .store_op = StoreOp::DontCare,
  };
  AttachmentDescription stencil = {
    .load_op = LoadOp::Invalid,
    .store_op = StoreOp::DontCare,
  };

  std::uint32_t layer_count = 1;
  std::uint32_t view_mask = 0;

  [[nodiscard]] auto get_colour_attachment_count() const
  {
    std::uint32_t n = 0;
    while (n < max_colour_attachments && color[n].load_op != LoadOp::Invalid) {
      n++;
    }
    return n;
  }
};

struct Framebuffer final
{
  struct AttachmentDescription
  {
    TextureHandle texture{};
    TextureHandle resolve_texture{};

    AttachmentDescription() = default;
    AttachmentDescription(const Holder<TextureHandle>& tex,
                          const Holder<TextureHandle>& res = {})
      : AttachmentDescription(*tex, *res)
    {
    }
    AttachmentDescription(const TextureHandle tex, const TextureHandle res = {})
      : texture(tex)
      , resolve_texture(res)
    {
    }
  };

  std::array<AttachmentDescription, max_colour_attachments> color{};
  AttachmentDescription depth_stencil{};

  std::string debug_name{};

  [[nodiscard]] auto get_colour_attachment_count() const
  {
    std::uint32_t n = 0;
    while (n < max_colour_attachments && color[n].texture) {
      n++;
    }
    return n;
  }
};

struct Viewport
{
  float x = 0.0f;
  float y = 0.0f;
  float width = 1.0f;
  float height = 1.0f;
  float minDepth = 1.0f;
  float maxDepth = 0.0f;
};

struct ScissorRect
{
  std::uint32_t x = 0;
  std::uint32_t y = 0;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

struct StencilState
{
  bool enabled{ false };
  StencilOp stencil_failure_operation = StencilOp::Keep;
  StencilOp depth_failure_operation = StencilOp::Keep;
  StencilOp depth_stencil_pass_operation = StencilOp::Keep;
  CompareOp stencil_compare_op = CompareOp::AlwaysPass;
  std::uint32_t read_mask = static_cast<std::uint32_t>(~0);
  std::uint32_t write_mask = static_cast<std::uint32_t>(~0);
};

struct DepthState
{
  CompareOp compare_operation{ CompareOp::AlwaysPass };
  bool is_depth_test_enabled{ true };
  bool is_depth_write_enabled{ false };
};

struct TextureLayers
{
  std::uint32_t mip_level = 0;
  std::uint32_t layer = 0;
  std::uint32_t num_layers = 1;
};

struct Offset3D
{
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t z = 0;
};

#define MAKE_BIT_FIELD(E)                                                      \
  constexpr E operator|(const E lhs, const E rhs)                              \
  {                                                                            \
    const auto underlying_lhs = std::to_underlying(lhs);                       \
    const auto underlying_rhs = std::to_underlying(rhs);                       \
    return static_cast<E>(underlying_lhs | underlying_rhs);                    \
  }                                                                            \
  constexpr E operator&(const E lhs, const E rhs)                              \
  {                                                                            \
    const auto underlying_lhs = std::to_underlying(lhs);                       \
    const auto underlying_rhs = std::to_underlying(rhs);                       \
    return static_cast<E>(underlying_lhs & underlying_rhs);                    \
  }                                                                            \
  constexpr bool operator!(const E value)                                      \
  {                                                                            \
    return std::to_underlying(value) == 0;                                     \
  }                                                                            \
  constexpr bool operator==(const E lhs, const E rhs)                          \
  {                                                                            \
    return std::to_underlying(lhs) == std::to_underlying(rhs);                 \
  }                                                                            \
  constexpr bool operator!=(const E lhs, const E rhs)                          \
  {                                                                            \
    return std::to_underlying(lhs) != std::to_underlying(rhs);                 \
  }                                                                            \
  constexpr E& operator|=(E& lhs, const E rhs)                                 \
  {                                                                            \
    lhs = lhs | rhs;                                                           \
    return lhs;                                                                \
  }                                                                            \
  constexpr E& operator&=(E& lhs, const E rhs)                                 \
  {                                                                            \
    lhs = lhs & rhs;                                                           \
    return lhs;                                                                \
  }                                                                            \
  constexpr E operator~(const E value)                                         \
  {                                                                            \
    return static_cast<E>(~std::to_underlying(value));                         \
  }

struct BoundingBox
{
  glm::vec3 minimum{ std::numeric_limits<float>::max() };
  glm::vec3 maximum{ std::numeric_limits<float>::lowest() };
  constexpr auto min() const -> const auto& { return minimum; }
  constexpr auto max() const -> const auto& { return maximum; }

  BoundingBox() = default;
  BoundingBox(const glm::vec3& mini, const glm::vec3& maxi)
    : minimum(mini)
    , maximum(maxi)
  {
  }

  auto expand(const glm::vec3& point)
  {
    minimum = glm::min(minimum, point);
    maximum = glm::max(maximum, point);
  }
  auto expand(const BoundingBox& aabb)
  {
    expand(aabb.min());
    expand(aabb.max());
  }

  constexpr auto is_valid() const
  {
    return minimum.x <= maximum.x && minimum.y <= maximum.y &&
           minimum.z <= maximum.z;
  }
};

struct SubmitHandle
{
  std::uint32_t buffer_index{ 0 };
  std::uint32_t submit_identifier{ 0 };

  SubmitHandle() = default;
  explicit SubmitHandle(const std::uint64_t handle)
    : buffer_index(static_cast<std::uint32_t>(handle & 0xffffffff))
    , submit_identifier(static_cast<std::uint32_t>(handle >> 32))
  {
  }

  [[nodiscard]] auto empty() const { return submit_identifier == 0; }
  [[nodiscard]] auto handle() const
  {
    return (static_cast<std::uint64_t>(submit_identifier) << 32) + buffer_index;
  }
};

static constexpr auto get_aligned_size = [](const auto s,
                                            const auto alignment) {
  return (s + alignment - 1) & ~(alignment - 1);
};

enum class TextureUsageBits : std::uint8_t
{
  Sampled = bit(0),
  Storage = bit(1),
  Attachment = bit(2),
};
BIT_FIELD(TextureUsageBits)

enum class StorageType : std::uint8_t
{
  Device = bit(0),
  Transient = bit(1),
  HostVisible = bit(2),
};
BIT_FIELD(StorageType)

inline auto
storage_type_to_vk_memory_property_flags(StorageType storage)
  -> VkMemoryPropertyFlags
{
  VkMemoryPropertyFlags memory_flags{ 0 };

  switch (storage) {
    case StorageType::Device:
      memory_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      break;
    case StorageType::HostVisible:
      memory_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
      break;
    case StorageType::Transient:
      memory_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                      VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
      break;
  }
  return memory_flags;
}

auto
set_name(const IContext&, const std::uint64_t, VkObjectType, std::string_view)
  -> void;
template<typename... Args>
inline auto
set_name(const IContext& ctx,
         const auto vk_object,
         const VkObjectType type,
         const std::format_string<Args...> fmt,
         Args&&... args) -> void
{
  static_assert(std::is_pointer_v<decltype(vk_object)>);
  return set_name(ctx,
                  reinterpret_cast<std::uint64_t>(vk_object),
                  type,
                  std::format(fmt, std::forward<Args>(args)...));
}

auto
format_to_vk_format(const Format format) -> VkFormat;
auto
vk_format_to_format(VkFormat format) -> Format;
auto
format_is_depth(const VkFormat format) -> bool;
auto
format_is_stencil(const VkFormat format) -> bool;

inline auto
is_depth_or_stencil_format(const VkFormat format) -> bool
{
  return format_is_depth(format) || format_is_stencil(format);
}
inline auto
is_depth_or_stencil_format(const Format format) -> bool
{
  return is_depth_or_stencil_format(format_to_vk_format(format));
}

constexpr auto generate_n = [](const auto count, auto&& fn) {
  using retval = std::invoke_result_t<decltype(fn), int>;
  using count_type = decltype(count);
  const auto size_count = static_cast<count_type>(count);
  return std::views::iota(static_cast<count_type>(0), size_count) |
         std::views::transform(
           [f = std::move(fn)](const count_type i) { return f(i); }) |
         std::ranges::to<std::vector<retval>>();
};

}