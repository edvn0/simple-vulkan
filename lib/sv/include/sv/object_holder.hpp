#pragma once

#include "sv/object_handle.hpp"
#include <utility>

namespace sv {

struct IContext;

namespace destruction {
auto
context_destroy(IContext*, TextureHandle) -> void;
auto
context_destroy(IContext*, BufferHandle) -> void;
auto
context_destroy(IContext*, GraphicsPipelineHandle) -> void;
auto
context_destroy(IContext*, ComputePipelineHandle) -> void;
auto
context_destroy(IContext*, ShaderModuleHandle) -> void;
auto
context_destroy(IContext*, SamplerHandle) -> void;
}

template<typename T>
concept CanBeDestroyed = requires(IContext* context, T handle) {
  destruction::context_destroy(context, handle);
};

template<CanBeDestroyed HandleType>
class Holder final
{
public:
  Holder() noexcept = default;

  explicit Holder(IContext* ctx, HandleType handle) noexcept
    : context(ctx)
    , handle(handle)
  {
  }

  ~Holder() noexcept { destruction::context_destroy(context, handle); }

  Holder(const Holder&) = delete;
  Holder(Holder&& other) noexcept
    : context(other.context)
    , handle(std::exchange(other.handle, HandleType{}))
  {
    other.context = nullptr;
  }

  auto operator=(const Holder&) noexcept -> Holder& = delete;

  auto operator=(Holder&& other) noexcept -> Holder&
  {
    if (this != &other) {
      destruction::context_destroy(context, handle);
      context = other.context;
      handle = std::exchange(other.handle, HandleType{});
      other.context = nullptr;
    }
    return *this;
  }

  auto operator=(std::nullptr_t) noexcept -> Holder&
  {
    if (context) {
      reset();
    }
    return *this;
  }

  explicit operator HandleType() const noexcept { return handle; }
  auto operator*() const noexcept { return handle; }

  [[nodiscard]] auto valid() const noexcept -> bool { return handle.valid(); }
  [[nodiscard]] auto empty() const noexcept -> bool { return handle.empty(); }

  auto reset() noexcept -> void
  {
    destruction::context_destroy(context, handle);
    context = nullptr;
    handle = HandleType{};
  }

  auto release() noexcept -> HandleType
  {
    context = nullptr;
    return std::exchange(handle, HandleType{});
  }

  [[nodiscard]] auto index() const noexcept -> std::uint32_t
  {
    return handle.index();
  }
  [[nodiscard]] auto generation() const noexcept -> std::uint32_t
  {
    return handle.generation();
  }

  template<typename V = void*>
  [[nodiscard]] auto explicit_cast() const -> V*
  {
    return handle.template explicit_cast<V>();
  }

  auto operator<=>(const Holder& other) const noexcept = default;

  static auto invalid() noexcept -> Holder
  {
    return Holder{ nullptr, HandleType{} };
  }

private:
  IContext* context{ nullptr };
  HandleType handle;
};
}