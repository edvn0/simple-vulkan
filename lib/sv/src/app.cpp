#include "sv/app.hpp"

#include "sv/common.hpp"
#include "sv/context.hpp"
#include "sv/renderer.hpp"

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
  VkSemaphoreWaitInfo wi{ VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
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
  : allocator(std::move(rhs.allocator))
  , window(std::move(rhs.window))
  , owns_windowing_system(std::exchange(rhs.owns_windowing_system, false))
{
}

auto
App::operator=(App&& rhs) noexcept -> App&
{
  if (this == &rhs)
    return *this;
  if (owns_windowing_system)
    glfwTerminate();
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

  return std::expected<App, InitialisationError>{ std::in_place,
                                                  std::move(tmp) };
}

auto
App::create_command_pool(std::uint32_t family) -> bool
{
  VkCommandPoolCreateInfo ci{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
  ci.queueFamilyIndex = family;
  ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  if (vkCreateCommandPool(context->get_device(), &ci, nullptr, &command_pool) !=
      VK_SUCCESS)
    return false;

  context->enqueue_destruction([ptr = command_pool](IContext& ctx) {
    vkDestroyCommandPool(ctx.get_device(), ptr, nullptr);
  });
  return true;
}

auto
App::destroy_command_pool() -> void
{
  command_pool = VK_NULL_HANDLE;
}

auto
App::create_swapchain() -> bool
{

  vkb::SwapchainBuilder b{
    context->get_device_wrapper(),
    context->get_surface(),
  };
  auto ret =
    b.set_desired_min_image_count(vkb::SwapchainBuilder::DOUBLE_BUFFERING)
      .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
      .build();
  if (!ret)
    return false;
  swapchain = ret.value();

  auto imgs = swapchain.get_images();
  auto vws = swapchain.get_image_views();
  if (!imgs || !vws)
    return false;

  images = std::move(imgs.value());
  views = std::move(vws.value());
  swapchain_format = swapchain.image_format;
  swapchain_extent = swapchain.extent;
  return true;
}

auto
App::destroy_swapchain() -> void
{
  if (context) {
    for (auto v : views)
      vkDestroyImageView(context->get_device(), v, nullptr);
  }
  views.clear();
  images.clear();
  if (swapchain.swapchain)
    vkb::destroy_swapchain(swapchain);
  swapchain = {};
}

auto
App::create_frame_sync(uint32_t in_flight) -> bool
{
  VkSemaphoreTypeCreateInfo ti{ VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
  ti.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
  ti.initialValue = 0;
  VkSemaphoreCreateInfo tci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &ti };
  if (vkCreateSemaphore(
        context->get_device(), &tci, nullptr, &timeline.render_timeline) !=
      VK_SUCCESS)
    return false;
  context->enqueue_destruction([ptr = timeline.render_timeline](IContext& ctx) {
    vkDestroySemaphore(ctx.get_device(), ptr, nullptr);
  });
  timeline.next_value = 1;

  frames.resize(in_flight);
  VkSemaphoreCreateInfo sci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

  for (auto& f : frames) {
    if (vkCreateSemaphore(context->get_device(), &sci, nullptr, &f.acquire) !=
        VK_SUCCESS)
      return false;
    if (vkCreateSemaphore(context->get_device(), &sci, nullptr, &f.present) !=
        VK_SUCCESS)
      return false;

    VkCommandBufferAllocateInfo ai{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO
    };
    ai.commandPool = command_pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(context->get_device(), &ai, &f.cmd) !=
        VK_SUCCESS)
      return false;

    context->enqueue_destruction([ptr = f.acquire](IContext& ctx) {
      vkDestroySemaphore(ctx.get_device(), ptr, nullptr);
    });
    context->enqueue_destruction([ptr = f.present](IContext& ctx) {
      vkDestroySemaphore(ctx.get_device(), ptr, nullptr);
    });
    f.render_done_value = 0;
  }
  frame_cursor = 0;
  return true;
}

auto
App::destroy_frame_sync() -> void
{
  frames.clear();
  timeline = {};
}

auto
App::attach_context(IContext& ctx) -> bool
{
  context = &ctx;
  if (!create_command_pool(context->get_graphics_queue_family()))
    return false;
  if (!create_swapchain())
    return false;
  if (!create_frame_sync(2))
    return false;
  return true;
}

auto
App::detach_context() -> void
{
  if (!context)
    return;
  vkQueueWaitIdle(context->get_graphics_queue());
  vkDeviceWaitIdle(context->get_device());
  destroy_frame_sync();
  destroy_swapchain();
  destroy_command_pool();
  context = nullptr;
}

auto
App::acquire_frame() -> std::optional<AcquiredFrame>
{
  auto& f = frames[frame_cursor];

  wait_frame_done(context->get_device(), timeline, f);

  uint32_t index{};
  auto res = vkAcquireNextImageKHR(context->get_device(),
                                   swapchain.swapchain,
                                   UINT64_MAX,
                                   f.acquire,
                                   VK_NULL_HANDLE,
                                   &index);
  if (res == VK_ERROR_OUT_OF_DATE_KHR) {
    destroy_swapchain();
    create_swapchain();
    return std::nullopt;
  }
  if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
    return std::nullopt;

  AcquiredFrame af{};
  af.image_index = index;
  af.image = images[index];
  af.view = views[index];
  af.extent = swapchain_extent;
  af.acquire = f.acquire;
  af.present = f.present;
  return af;
}

auto
App::command_buffer_for_frame() -> VkCommandBuffer
{
  return frames[frame_cursor].cmd;
}

auto
App::submit_frame(const AcquiredFrame& af) -> bool
{
  auto& f = frames[frame_cursor];

  VkCommandBufferSubmitInfo cb{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
  cb.commandBuffer = f.cmd;

  VkSemaphoreSubmitInfo w_acq{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
  w_acq.semaphore = af.acquire;
  w_acq.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

  const uint64_t signal_value = timeline.next_value++;

  VkSemaphoreSubmitInfo s_timeline{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
  s_timeline.semaphore = timeline.render_timeline;
  s_timeline.value = signal_value;
  s_timeline.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

  VkSemaphoreSubmitInfo s_present{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
  s_present.semaphore = af.present;
  s_present.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

  VkSubmitInfo2 si{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
  si.waitSemaphoreInfoCount = 1;
  si.pWaitSemaphoreInfos = &w_acq;
  si.commandBufferInfoCount = 1;
  si.pCommandBufferInfos = &cb;
  VkSemaphoreSubmitInfo signals[2] = { s_timeline, s_present };
  si.signalSemaphoreInfoCount = 2;
  si.pSignalSemaphoreInfos = signals;

  vkQueueSubmit2(context->get_graphics_queue(), 1, &si, VK_NULL_HANDLE);

  VkPresentInfoKHR pi{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
  pi.waitSemaphoreCount = 1;
  pi.pWaitSemaphores = &af.present;
  VkSwapchainKHR sc = swapchain.swapchain;
  pi.swapchainCount = 1;
  pi.pSwapchains = &sc;
  pi.pImageIndices = &af.image_index;

  auto pr = vkQueuePresentKHR(context->get_present_queue(), &pi);

  f.render_done_value = signal_value;

  if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR) {
    vkQueueWaitIdle(context->get_present_queue());
    destroy_swapchain();
    create_swapchain();
    frame_cursor = (frame_cursor + 1) % static_cast<uint32_t>(frames.size());
    return false;
  }
  frame_cursor = (frame_cursor + 1) % static_cast<uint32_t>(frames.size());
  return pr == VK_SUCCESS;
}
}