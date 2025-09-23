#pragma once

#include "sv/object_handle.hpp"
#include "sv/object_holder.hpp"
#include "vulkan/vulkan_core.h"
#include <VkBootstrap.h>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <tuple>

namespace sv {

struct PimplDeleter
{
  template<typename T>
  auto operator()(const T* t)
  {
    delete t;
  }
};

enum class PresentMode : std::uint8_t
{
  FIFO,
  Mailbox
};

struct ApplicationConfiguration
{
  bool fail_on_any_error{ false };
  bool enable_fullscreen_switching{ false };
  PresentMode mode{ PresentMode::FIFO };
  std::optional<std::tuple<std::uint32_t, std::uint32_t>>
    extent_if_not_fullscreen{ std::make_tuple(1280, 800) };
};

class App;
struct Window
{
  bool fullscreen{
    false
  }; // This is the immutable flag, from some config or alike.
  std::uint32_t width{ 1280 };
  std::uint32_t height{ 800 };
  bool is_fullscreen{ fullscreen };
  App* owned_by{ nullptr };
  void* opaque_handle{ nullptr };

  auto extent() const { return std::make_tuple(width, height); }
};

struct InitialisationError
{
  enum class Code : std::uint8_t
  {
    None,
    WindowInit,
    WindowCreation,
  };

  Code code{ Code::None };
  std::string message;
};

struct IContext;
struct IRenderer;

class App
{

  ApplicationConfiguration app_config{};
  bool owns_windowing_system{ true };

  struct TrackingAllocator;
  std::unique_ptr<TrackingAllocator, PimplDeleter> allocator{ nullptr };
  std::unique_ptr<Window> window{ nullptr };
  IContext* context{ nullptr };
  IRenderer* renderer{ nullptr };

  App(const ApplicationConfiguration&, std::unique_ptr<Window>);

public:
  ~App();
  App(App&& rhs) noexcept;
  auto operator=(App&& rhs) noexcept -> App&;

  [[nodiscard]] auto should_close() const -> bool;
  auto poll_events() const -> void;

  auto get_window() const -> const auto& { return *window; }
  auto get_window() -> auto& { return *window; }

  static auto create(const ApplicationConfiguration&)
    -> std::expected<App, InitialisationError>;

  auto detach_context() -> void;
};

}