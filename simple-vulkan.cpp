#include "sv/abstract_context.hpp"
#include "sv/app.hpp"
#include "sv/camera.hpp"
#include "sv/context.hpp"
#include "sv/event_system.hpp"
#include "sv/mesh_definition.hpp"
#include "sv/renderer.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <ranges>

extern auto
glfw_key_to_imgui_key(std::int32_t key) -> ImGuiKey;

namespace {
class SwapchainResizeHandler final
  : public sv::EventSystem::TypedEventHandler<
      sv::EventSystem::FramebufferSizeEvent>
{
  sv::App* app{};
  sv::VulkanContext* context{};

public:
  SwapchainResizeHandler(sv::App* a, sv::VulkanContext* c)
    : app(a)
    , context(c)
  {
  }
  [[nodiscard]] auto get_priority() const -> int override { return 900; }

protected:
  auto handle_event(const sv::EventSystem::FramebufferSizeEvent& e)
    -> bool override
  {
    app->get_window().width = static_cast<std::uint32_t>(e.width);
    app->get_window().height = static_cast<std::uint32_t>(e.height);
    context->resize_next_frame();
    return false;
  }
};

class CameraInputHandler final
  : public sv::EventSystem::TypedEventHandler<sv::EventSystem::KeyEvent,
                                              sv::EventSystem::MouseMoveEvent,
                                              sv::EventSystem::MouseButtonEvent>
{
  GLFWwindow* window{};
  sv::FirstPersonCameraBehaviour* behaviour{};
  bool mouse_held{ false };
  glm::vec2 mouse_norm{ 0 };

public:
  explicit CameraInputHandler(void* win, sv::FirstPersonCameraBehaviour* b)
    : window(static_cast<GLFWwindow*>(win))
    , behaviour(b)
  {
  }
  [[nodiscard]] auto get_priority() const -> int override { return 800; }

protected:
  auto handle_event(const sv::EventSystem::KeyEvent& e) -> bool override
  {
    if (ImGui::GetIO().WantCaptureKeyboard)
      return false;
    const bool pressed = e.action != GLFW_RELEASE;
    switch (e.key) {
      case GLFW_KEY_W:
        behaviour->movement.forward = pressed;
        break;
      case GLFW_KEY_S:
        behaviour->movement.backward = pressed;
        break;
      case GLFW_KEY_A:
        behaviour->movement.left = pressed;
        break;
      case GLFW_KEY_D:
        behaviour->movement.right = pressed;
        break;
      case GLFW_KEY_E:
        behaviour->movement.up = pressed;
        break;
      case GLFW_KEY_Q:
        behaviour->movement.down = pressed;
        break;
      case GLFW_KEY_LEFT_SHIFT:
        behaviour->movement.fast_speed = pressed;
        break;
      default:
        break;
    }
    return false;
  }
  auto handle_event(const sv::EventSystem::MouseButtonEvent& e) -> bool override
  {
    if (ImGui::GetIO().WantCaptureMouse)
      return false;
    if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
      mouse_held = (e.action == GLFW_PRESS);
      glfwSetInputMode(window,
                       GLFW_CURSOR,
                       mouse_held ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
      if (mouse_held)
        behaviour->mouse_position = mouse_norm;
    }
    return mouse_held;
  }
  auto handle_event(const sv::EventSystem::MouseMoveEvent& e) -> bool override
  {
    int w{}, h{};
    glfwGetFramebufferSize(window, &w, &h);
    if (w > 0 && h > 0) {
      mouse_norm = { static_cast<float>(e.x_pos) / static_cast<float>(w),
                     1.0f -
                       static_cast<float>(e.y_pos) / static_cast<float>(h) };
    }
    return mouse_held;
  }

public:
  auto tick(const double dt) const -> void
  {
    const bool imgui_block = ImGui::GetIO().WantCaptureMouse;
    behaviour->update(dt, mouse_norm, mouse_held && !imgui_block);
  }
};

static auto
setup_event_callbacks(void* w, sv::EventSystem::EventDispatcher* d) -> void
{
  auto* window = static_cast<GLFWwindow*>(w);

  glfwSetWindowUserPointer(window, d);
  glfwSetKeyCallback(
    window, [](GLFWwindow* win, int key, int scancode, int action, int mods) {
      auto* dispatcher = static_cast<sv::EventSystem::EventDispatcher*>(
        glfwGetWindowUserPointer(win));
      dispatcher->handle_key_callback(win, key, scancode, action, mods);

      if (key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(win, GLFW_TRUE);

      auto& io = ImGui::GetIO();
      io.AddKeyEvent(glfw_key_to_imgui_key(key), action != GLFW_RELEASE);
    });
  glfwSetMouseButtonCallback(
    window, [](GLFWwindow* win, int button, int action, int mods) {
      auto* dispatcher = static_cast<sv::EventSystem::EventDispatcher*>(
        glfwGetWindowUserPointer(win));
      dispatcher->handle_mouse_button_callback(win, button, action, mods);
      double xpos, ypos;
      glfwGetCursorPos(win, &xpos, &ypos);
      const ImGuiMouseButton_ imgui_button =
        (button == GLFW_MOUSE_BUTTON_LEFT)
          ? ImGuiMouseButton_Left
          : (button == GLFW_MOUSE_BUTTON_RIGHT ? ImGuiMouseButton_Right
                                               : ImGuiMouseButton_Middle);
      auto& io = ImGui::GetIO();
      io.AddMouseButtonEvent(imgui_button, action != GLFW_RELEASE);
    });
  glfwSetCursorPosCallback(window, [](GLFWwindow* win, double x, double y) {
    auto* dispatcher = static_cast<sv::EventSystem::EventDispatcher*>(
      glfwGetWindowUserPointer(win));
    dispatcher->handle_cursor_pos_callback(win, x, y);
    ImGui::GetIO().AddMousePosEvent(static_cast<float>(x),
                                    static_cast<float>(y));
  });
  glfwSetScrollCallback(window,
                        [](GLFWwindow*, double xoffset, double yoffset) {
                          ImGuiIO& io = ImGui::GetIO();
                          io.AddMouseWheelEvent(static_cast<float>(xoffset),
                                                static_cast<float>(yoffset));
                        });
  glfwSetWindowSizeCallback(window, [](GLFWwindow* win, int width, int height) {
    auto* dispatcher = static_cast<sv::EventSystem::EventDispatcher*>(
      glfwGetWindowUserPointer(win));
    dispatcher->handle_window_size_callback(win, width, height);
  });
  glfwSetFramebufferSizeCallback(window, [](GLFWwindow* win, int w, int h) {
    auto* dispatcher = static_cast<sv::EventSystem::EventDispatcher*>(
      glfwGetWindowUserPointer(win));
    dispatcher->handle_framebuffer_size_callback(win, w, h);
  });
}

auto
parse_mode(const std::string_view val)
{
  if (val == "fifo" || val == "FIFO")
    return sv::PresentMode::FIFO;
  if (val == "mailbox" || val == "MAILBOX" || val == "mm")
    return sv::PresentMode::Mailbox;

  return sv::PresentMode::Mailbox;
}
}

static auto
run(std::span<const std::string_view> args)
{
  using namespace sv;
  PresentMode mode{ PresentMode::FIFO };
  if (auto it = std::ranges::find(args, "mode"); it != std::ranges::end(args)) {
    auto next = std::ranges::next(it);

    if (next != std::ranges::end(args)) {
      mode = parse_mode(*next);
    }
  }

  auto maybe_app = App::create({
    .mode = mode,
  });
  if (!maybe_app)
    return 1;
  auto app = std::move(maybe_app.value());

  auto maybe_ctx = VulkanContext::create(app.get_window(),
                                         {
                                           .abort_on_validation_error = false,
                                         });
  if (!maybe_ctx)
    return 1;
  auto context = std::move(maybe_ctx.value());
  Renderer renderer{ *context, app.get_window().extent() };
  Camera camera(
    std::make_unique<FirstPersonCameraBehaviour>(glm::vec3{ 0, -6.0F, -3.0F },
                                                 glm::vec3{ 0, 0, 0.0F },
                                                 glm::vec3{ 0, 1, 0 }));
  sv::EventSystem::EventDispatcher event_dispatcher;
  setup_event_callbacks(app.get_window().opaque_handle, &event_dispatcher);

  const auto camera_input = std::make_shared<CameraInputHandler>(
    app.get_window().opaque_handle,
    dynamic_cast<FirstPersonCameraBehaviour*>(camera.get_behaviour()));

  event_dispatcher.subscribe<EventSystem::KeyEvent,
                             EventSystem::MouseMoveEvent,
                             EventSystem::MouseButtonEvent>(camera_input);
  const auto fb_resize = std::make_shared<SwapchainResizeHandler>(
    &app, static_cast<VulkanContext*>(context.get()));
  event_dispatcher.subscribe<sv::EventSystem::FramebufferSizeEvent>(fb_resize);

  auto load = load_mesh_data("meshes/cube.obj");
  save_mesh_data("meshes/cube.cache.obj", *load);
  auto load_cache = load_mesh_file("meshes/cube.cache.obj");

  auto cube = *RenderMesh::create(*context, "meshes/cube.cache.obj");

  double last_time = glfwGetTime();
  while (!app.should_close()) {
    event_dispatcher.process_events();

    const double now = glfwGetTime();
    const double dt = now - last_time;
    last_time = now;
    camera_input->tick(dt);

    auto sc_result = context->recreate_swapchain(app.get_window().width,
                                                 app.get_window().height);

    if (sc_result == IContext::SwapchainRecreateResult::Success) {
      renderer.resize(app.get_window().width, app.get_window().height);
    }

    renderer.begin_frame(camera);
    auto& cmd = context->acquire_command_buffer();
    renderer.submit(cube, glm::mat4{ 1.0F }, 0, 0);
    auto scale = glm::translate(
      glm::scale(glm::mat4{ 1.0F }, glm::vec3{ 100.0F, 0.1F, 100.F }),
      glm::vec3{ 0, 5, 0 });
    renderer.submit(cube, scale, 0, 0);
    renderer.record(cmd, context->get_current_swapchain_texture());
    context->submit(cmd, context->get_current_swapchain_texture());
  }

  app.detach_context();
  return 0;
}

int
main(int argc, char** argv)
{
  auto views = std::views::iota(0, argc) |
               std::views::transform([args = argv](const auto i) {
                 return std::string_view(args[i]);
               }) |
               std::ranges::to<std::vector<std::string_view>>();
  auto done = run(views);

  return done;
}
