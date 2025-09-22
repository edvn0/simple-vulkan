#include "sv/app.hpp"

#include "sv/bindless.hpp"
#include "sv/common.hpp"
#include "sv/context.hpp"
#include "sv/renderer.hpp"
#include "sv/texture.hpp"
#include "vulkan/vulkan_core.h"

#include <GLFW/glfw3.h>
#include <array>
#include <iostream>
#include <mutex>
#include <unordered_map>

namespace sv {

namespace {

auto
wait_frame_done(VkDevice device, const auto& t, const auto& f) -> void
{
  if (f.render_done_value == 0)
    return;
  VkSemaphoreWaitInfo wi{};
  wi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
  wi.semaphoreCount = 1;
  VkSemaphore sem = t.render_timeline;
  wi.pSemaphores = &sem;
  wi.pValues = &f.render_done_value;
  vkWaitSemaphores(device, &wi, UINT64_MAX);
}

auto
glfw_last_error() -> std::string
{
  const char* desc = nullptr;
  glfwGetError(&desc);
  return desc ? std::string{ desc } : std::string{};
}
}

struct App::TrackingAllocator
{
  struct State
  {
    std::mutex m;
    std::unordered_map<void*, size_t> sizes;
    std::atomic<size_t> alloc_count{ 0 };
    std::atomic<size_t> realloc_count{ 0 };
    std::atomic<size_t> free_count{ 0 };
    std::atomic<size_t> bytes_current{ 0 };
    std::atomic<size_t> bytes_peak{ 0 };
    std::atomic<size_t> bytes_total{ 0 };
    std::ostream* out{ &std::cerr };
    std::string tag{ "GLFW" };
    bool verbose{ false };
  };

  std::unique_ptr<State> state{ std::make_unique<State>() };

  ~TrackingAllocator()
  {
    if (state->sizes.size() > 0) {
      __debugbreak();
    }
  }

  static void* allocate(size_t sz, void* user) noexcept
  {
    auto* s = static_cast<State*>(user);
    void* p = std::malloc(sz);
    if (!p)
      return nullptr;
    std::scoped_lock lk(s->m);
    s->sizes.emplace(p, sz);
    s->alloc_count++;
    s->bytes_current += sz;
    s->bytes_total += sz;
    s->bytes_peak = std::max(s->bytes_peak.load(), s->bytes_current.load());
    if (s->verbose)
      (*s->out) << '[' << s->tag << "] alloc   " << p << ' ' << sz << '\n';
    return p;
  }

  static void* reallocate(void* block, size_t sz, void* user) noexcept
  {
    auto* s = static_cast<State*>(user);
    if (!block)
      return allocate(sz, user);
    if (sz == 0) {
      deallocate(block, user);
      return nullptr;
    }

    size_t old_sz = 0;
    {
      std::scoped_lock lk(s->m);
      if (auto it = s->sizes.find(block); it != s->sizes.end())
        old_sz = it->second;
    }
    void* p2 = std::realloc(block, sz);
    if (!p2)
      return nullptr;

    std::scoped_lock lk(s->m);
    if (old_sz) {
      s->bytes_current -= old_sz;
      s->sizes.erase(block);
    }
    s->sizes.emplace(p2, sz);
    s->bytes_current += sz;
    if (sz > old_sz)
      s->bytes_total += (sz - old_sz);
    s->bytes_peak = std::max(s->bytes_peak.load(), s->bytes_current.load());
    s->realloc_count++;
    if (s->verbose)
      (*s->out) << '[' << s->tag << "] realloc " << block << " -> " << p2 << ' '
                << sz << '\n';
    return p2;
  }

  static void deallocate(void* block, void* user) noexcept
  {
    if (!block)
      return;
    auto* s = static_cast<State*>(user);
    size_t sz = 0;
    {
      std::scoped_lock lk(s->m);
      if (auto it = s->sizes.find(block); it != s->sizes.end()) {
        sz = it->second;
        s->sizes.erase(it);
        s->bytes_current -= sz;
      }
      s->free_count++;
      if (s->verbose)
        (*s->out) << '[' << s->tag << "] free    " << block << ' ' << sz
                  << '\n';
    }
    std::free(block);
  }

  auto to_glfw() const -> GLFWallocator
  {
    GLFWallocator a{};
    a.allocate = &allocate;
    a.reallocate = &reallocate;
    a.deallocate = &deallocate;
    a.user = state.get();
    return a;
  }

  auto user() const -> void* { return state.get(); }

  auto set_stream(std::ostream& os) const -> void { state->out = &os; }
  auto set_verbose(bool v) const -> void { state->verbose = v; }
  auto set_tag(const auto& t) -> void { state->tag = std::move(t); }

  auto dump(std::ostream* os = nullptr) const -> void
  {
    auto* out = os ? os : state->out;
    std::scoped_lock lk(state->m);
    (*out) << '[' << state->tag << "] "
           << "allocs=" << state->alloc_count.load()
           << " reallocs=" << state->realloc_count.load()
           << " frees=" << state->free_count.load()
           << " live_bytes=" << state->bytes_current.load()
           << " peak_bytes=" << state->bytes_peak.load()
           << " total_bytes=" << state->bytes_total.load()
           << " leaks=" << state->sizes.size() << '\n';
  }
};

App::App(const ApplicationConfiguration& conf, std::unique_ptr<Window> w)
  : app_config(conf)
  , allocator(
      std::unique_ptr<TrackingAllocator, PimplDeleter>(new TrackingAllocator,
                                                       PimplDeleter{}))
  , window(std::move(w))
{
  window->owned_by = this;
}

App::App(App&& rhs) noexcept
  : app_config(rhs.app_config)
  , owns_windowing_system(std::exchange(rhs.owns_windowing_system, false))
  , allocator(std::move(rhs.allocator))
  , window(std::move(rhs.window))
{
}

auto
App::operator=(App&& rhs) noexcept -> App&
{
  if (this == &rhs)
    return *this;
  if (owns_windowing_system)
    glfwTerminate();
  app_config = rhs.app_config;
  allocator = std::move(rhs.allocator);
  window = std::move(rhs.window);
  owns_windowing_system = std::exchange(rhs.owns_windowing_system, false);
  return *this;
}

App::~App()
{
  if (owns_windowing_system)
    glfwTerminate();
  if (allocator)
    allocator->dump();
}

auto
App::should_close() const -> bool
{
  static auto* glfw = static_cast<GLFWwindow*>(window->opaque_handle);
  return glfwWindowShouldClose(glfw);
}

auto
App::poll_events() const -> void
{
  glfwPollEvents();
}

auto
App::create(const ApplicationConfiguration& config)
  -> std::expected<App, InitialisationError>
{
  using enum InitialisationError::Code;

  auto w = std::make_unique<Window>();
  auto tmp = App{ config, std::move(w) };

  auto&& [ww, hh] =
    config.extent_if_not_fullscreen.value_or(std::make_tuple(1280, 1024));

  tmp.allocator->set_verbose(true);
  tmp.allocator->set_stream(std::cerr);
  tmp.allocator->set_tag("GLFW");
  tmp.window->width = ww;
  tmp.window->height = hh;

  auto alloc = tmp.allocator->to_glfw();
  glfwInitAllocator(&alloc);

  if (!glfwInit()) {
    auto msg = glfw_last_error();
    return make_error<InitialisationError>(WindowInit, msg);
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  auto window = glfwCreateWindow(ww, hh, "SimpleVK", nullptr, nullptr);
  if (nullptr == window) {
    auto msg = glfw_last_error();
    return make_error<InitialisationError>(WindowCreation, msg);
  }
  tmp.window->opaque_handle = window;

  glfwSetKeyCallback(
    window, +[](GLFWwindow* w, int key, int, int, int) -> void {
      if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(w, GLFW_TRUE);
      }
    });

  return std::expected<App, InitialisationError>{ std::in_place,
                                                  std::move(tmp) };
}

auto
App::attach_context(IContext& ctx, IRenderer& r) -> bool
{
  context = &ctx;
  renderer = &r;

  glfwSetWindowUserPointer(static_cast<GLFWwindow*>(window->opaque_handle),
                           this);

  glfwSetFramebufferSizeCallback(
    static_cast<GLFWwindow*>(window->opaque_handle),
    [](GLFWwindow* win, int w, int h) -> void {
      auto* app = static_cast<App*>(glfwGetWindowUserPointer(win));

      auto* context = static_cast<VulkanContext*>(app->context);
      app->window->width = static_cast<std::uint32_t>(w);
      app->window->height = static_cast<std::uint32_t>(h);

      context->resize_next_frame();
    });

  return true;
}

auto
App::detach_context() -> void
{
  if (!context)
    return;
  vkQueueWaitIdle(context->get_graphics_queue());
  vkDeviceWaitIdle(context->get_device());
  context = nullptr;
}

}