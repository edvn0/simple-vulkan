#pragma once

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
    extent_if_not_fullscreen{ std::make_tuple(1280, 1024) };
};

class App;
struct Window
{
  bool fullscreen{
    false
  }; // This is the immutable flag, from some config or alike.
  std::uint32_t width{ 1280 };
  std::uint32_t height{ 1024 };
  bool is_fullscreen{ fullscreen };
  App* owned_by{ nullptr };
  void* opaque_handle{ nullptr };
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

struct AcquiredFrame
{
  std::uint32_t image_index{};
  VkImage image{};
  VkImageView view{};
  VkExtent2D extent{};
  VkSemaphore acquire{};
  VkSemaphore present{};
  VkFence cpu_fence{};
};

struct IContext;
class App
{

  ApplicationConfiguration app_config{};
  bool owns_windowing_system{ true };

  struct TrackingAllocator;
  std::unique_ptr<TrackingAllocator, PimplDeleter> allocator{ nullptr };
  std::unique_ptr<Window> window{ nullptr };
  IContext* context{ nullptr };
  vkb::Swapchain swapchain{};
  std::vector<VkImage> images;
  std::vector<VkImageView> views;
  VkFormat swapchain_format{};
  VkExtent2D swapchain_extent{};
  struct FrameSync
  {
    VkSemaphore acquire{};
    std::uint64_t render_done_value{};
  };

  struct TimelineSync
  {
    VkSemaphore render_timeline{};
    std::uint64_t next_value{};
  };

  std::vector<VkSemaphore> image_present_sems;
  std::vector<FrameSync> frames;
  TimelineSync timeline{};
  std::uint32_t frame_cursor{};

  auto create_command_pool(std::uint32_t family) -> bool;
  auto destroy_command_pool() -> void;

  auto create_swapchain() -> bool;
  auto destroy_swapchain() -> void;

  auto create_frame_sync(std::uint32_t in_flight) -> bool;
  auto destroy_frame_sync() -> void;

  App(const ApplicationConfiguration&, std::unique_ptr<Window>);

public:
  ~App();
  App(App&& rhs) noexcept;
  auto operator=(App&& rhs) noexcept -> App&;

  [[nodiscard]] auto should_close() const -> bool;
  auto poll_events() const -> void;

  auto get_window() const { return *window; }

  static auto create(const ApplicationConfiguration&)
    -> std::expected<App, InitialisationError>;

  auto attach_context(IContext&) -> bool;
  auto detach_context() -> void;

  auto acquire_frame() -> std::optional<AcquiredFrame>;
  auto command_buffer_for_frame() -> std::optional<VkCommandBuffer>;
  auto submit_frame(const AcquiredFrame&) -> bool;

  auto get_swapchain_extent() const -> VkExtent2D { return swapchain_extent; }
  auto get_swapchain_format() const -> VkFormat { return swapchain_format; }
};

}