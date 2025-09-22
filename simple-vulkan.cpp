#include "sv/abstract_context.hpp"
#include "sv/app.hpp"
#include "sv/camera.hpp"
#include "sv/context.hpp"
#include "sv/event_system.hpp"
#include "sv/renderer.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <ranges>

namespace {
auto
glfw_key_to_imgui_key(std::int32_t key) -> ImGuiKey
{
  switch (key) {
    // Control keys
    case GLFW_KEY_TAB:
      return ImGuiKey_Tab;
    case GLFW_KEY_LEFT:
      return ImGuiKey_LeftArrow;
    case GLFW_KEY_RIGHT:
      return ImGuiKey_RightArrow;
    case GLFW_KEY_UP:
      return ImGuiKey_UpArrow;
    case GLFW_KEY_DOWN:
      return ImGuiKey_DownArrow;
    case GLFW_KEY_PAGE_UP:
      return ImGuiKey_PageUp;
    case GLFW_KEY_PAGE_DOWN:
      return ImGuiKey_PageDown;
    case GLFW_KEY_HOME:
      return ImGuiKey_Home;
    case GLFW_KEY_END:
      return ImGuiKey_End;
    case GLFW_KEY_INSERT:
      return ImGuiKey_Insert;
    case GLFW_KEY_DELETE:
      return ImGuiKey_Delete;
    case GLFW_KEY_BACKSPACE:
      return ImGuiKey_Backspace;
    case GLFW_KEY_SPACE:
      return ImGuiKey_Space;
    case GLFW_KEY_ENTER:
      return ImGuiKey_Enter;
    case GLFW_KEY_ESCAPE:
      return ImGuiKey_Escape;
    // Modifiers
    case GLFW_KEY_LEFT_CONTROL:
      return ImGuiKey_LeftCtrl;
    case GLFW_KEY_LEFT_SHIFT:
      return ImGuiKey_LeftShift;
    case GLFW_KEY_LEFT_ALT:
      return ImGuiKey_LeftAlt;
    case GLFW_KEY_LEFT_SUPER:
      return ImGuiKey_LeftSuper;
    case GLFW_KEY_RIGHT_CONTROL:
      return ImGuiKey_RightCtrl;
    case GLFW_KEY_RIGHT_SHIFT:
      return ImGuiKey_RightShift;
    case GLFW_KEY_RIGHT_ALT:
      return ImGuiKey_RightAlt;
    case GLFW_KEY_RIGHT_SUPER:
      return ImGuiKey_RightSuper;
    case GLFW_KEY_MENU:
      return ImGuiKey_Menu;
    // Numbers
    case GLFW_KEY_0:
      return ImGuiKey_0;
    case GLFW_KEY_1:
      return ImGuiKey_1;
    case GLFW_KEY_2:
      return ImGuiKey_2;
    case GLFW_KEY_3:
      return ImGuiKey_3;
    case GLFW_KEY_4:
      return ImGuiKey_4;
    case GLFW_KEY_5:
      return ImGuiKey_5;
    case GLFW_KEY_6:
      return ImGuiKey_6;
    case GLFW_KEY_7:
      return ImGuiKey_7;
    case GLFW_KEY_8:
      return ImGuiKey_8;
    case GLFW_KEY_9:
      return ImGuiKey_9;
    // Letters
    case GLFW_KEY_A:
      return ImGuiKey_A;
    case GLFW_KEY_B:
      return ImGuiKey_B;
    case GLFW_KEY_C:
      return ImGuiKey_C;
    case GLFW_KEY_D:
      return ImGuiKey_D;
    case GLFW_KEY_E:
      return ImGuiKey_E;
    case GLFW_KEY_F:
      return ImGuiKey_F;
    case GLFW_KEY_G:
      return ImGuiKey_G;
    case GLFW_KEY_H:
      return ImGuiKey_H;
    case GLFW_KEY_I:
      return ImGuiKey_I;
    case GLFW_KEY_J:
      return ImGuiKey_J;
    case GLFW_KEY_K:
      return ImGuiKey_K;
    case GLFW_KEY_L:
      return ImGuiKey_L;
    case GLFW_KEY_M:
      return ImGuiKey_M;
    case GLFW_KEY_N:
      return ImGuiKey_N;
    case GLFW_KEY_O:
      return ImGuiKey_O;
    case GLFW_KEY_P:
      return ImGuiKey_P;
    case GLFW_KEY_Q:
      return ImGuiKey_Q;
    case GLFW_KEY_R:
      return ImGuiKey_R;
    case GLFW_KEY_S:
      return ImGuiKey_S;
    case GLFW_KEY_T:
      return ImGuiKey_T;
    case GLFW_KEY_U:
      return ImGuiKey_U;
    case GLFW_KEY_V:
      return ImGuiKey_V;
    case GLFW_KEY_W:
      return ImGuiKey_W;
    case GLFW_KEY_X:
      return ImGuiKey_X;
    case GLFW_KEY_Y:
      return ImGuiKey_Y;
    case GLFW_KEY_Z:
      return ImGuiKey_Z;
    // Function keys
    case GLFW_KEY_F1:
      return ImGuiKey_F1;
    case GLFW_KEY_F2:
      return ImGuiKey_F2;
    case GLFW_KEY_F3:
      return ImGuiKey_F3;
    case GLFW_KEY_F4:
      return ImGuiKey_F4;
    case GLFW_KEY_F5:
      return ImGuiKey_F5;
    case GLFW_KEY_F6:
      return ImGuiKey_F6;
    case GLFW_KEY_F7:
      return ImGuiKey_F7;
    case GLFW_KEY_F8:
      return ImGuiKey_F8;
    case GLFW_KEY_F9:
      return ImGuiKey_F9;
    case GLFW_KEY_F10:
      return ImGuiKey_F10;
    case GLFW_KEY_F11:
      return ImGuiKey_F11;
    case GLFW_KEY_F12:
      return ImGuiKey_F12;
    case GLFW_KEY_F13:
      return ImGuiKey_F13;
    case GLFW_KEY_F14:
      return ImGuiKey_F14;
    case GLFW_KEY_F15:
      return ImGuiKey_F15;
    case GLFW_KEY_F16:
      return ImGuiKey_F16;
    case GLFW_KEY_F17:
      return ImGuiKey_F17;
    case GLFW_KEY_F18:
      return ImGuiKey_F18;
    case GLFW_KEY_F19:
      return ImGuiKey_F19;
    case GLFW_KEY_F20:
      return ImGuiKey_F20;
    case GLFW_KEY_F21:
      return ImGuiKey_F21;
    case GLFW_KEY_F22:
      return ImGuiKey_F22;
    case GLFW_KEY_F23:
      return ImGuiKey_F23;
    case GLFW_KEY_F24:
      return ImGuiKey_F24;
    // Symbols
    case GLFW_KEY_APOSTROPHE:
      return ImGuiKey_Apostrophe;
    case GLFW_KEY_COMMA:
      return ImGuiKey_Comma;
    case GLFW_KEY_MINUS:
      return ImGuiKey_Minus;
    case GLFW_KEY_PERIOD:
      return ImGuiKey_Period;
    case GLFW_KEY_SLASH:
      return ImGuiKey_Slash;
    case GLFW_KEY_SEMICOLON:
      return ImGuiKey_Semicolon;
    case GLFW_KEY_EQUAL:
      return ImGuiKey_Equal;
    case GLFW_KEY_LEFT_BRACKET:
      return ImGuiKey_LeftBracket;
    case GLFW_KEY_BACKSLASH:
      return ImGuiKey_Backslash;
    case GLFW_KEY_RIGHT_BRACKET:
      return ImGuiKey_RightBracket;
    case GLFW_KEY_GRAVE_ACCENT:
      return ImGuiKey_GraveAccent;
    // Locks + system
    case GLFW_KEY_CAPS_LOCK:
      return ImGuiKey_CapsLock;
    case GLFW_KEY_SCROLL_LOCK:
      return ImGuiKey_ScrollLock;
    case GLFW_KEY_NUM_LOCK:
      return ImGuiKey_NumLock;
    case GLFW_KEY_PRINT_SCREEN:
      return ImGuiKey_PrintScreen;
    case GLFW_KEY_PAUSE:
      return ImGuiKey_Pause;
    // Keypad
    case GLFW_KEY_KP_0:
      return ImGuiKey_Keypad0;
    case GLFW_KEY_KP_1:
      return ImGuiKey_Keypad1;
    case GLFW_KEY_KP_2:
      return ImGuiKey_Keypad2;
    case GLFW_KEY_KP_3:
      return ImGuiKey_Keypad3;
    case GLFW_KEY_KP_4:
      return ImGuiKey_Keypad4;
    case GLFW_KEY_KP_5:
      return ImGuiKey_Keypad5;
    case GLFW_KEY_KP_6:
      return ImGuiKey_Keypad6;
    case GLFW_KEY_KP_7:
      return ImGuiKey_Keypad7;
    case GLFW_KEY_KP_8:
      return ImGuiKey_Keypad8;
    case GLFW_KEY_KP_9:
      return ImGuiKey_Keypad9;
    case GLFW_KEY_KP_DECIMAL:
      return ImGuiKey_KeypadDecimal;
    case GLFW_KEY_KP_DIVIDE:
      return ImGuiKey_KeypadDivide;
    case GLFW_KEY_KP_MULTIPLY:
      return ImGuiKey_KeypadMultiply;
    case GLFW_KEY_KP_SUBTRACT:
      return ImGuiKey_KeypadSubtract;
    case GLFW_KEY_KP_ADD:
      return ImGuiKey_KeypadAdd;
    case GLFW_KEY_KP_ENTER:
      return ImGuiKey_KeypadEnter;
    case GLFW_KEY_KP_EQUAL:
      return ImGuiKey_KeypadEqual;
    default:
      return ImGuiKey_None;
  }
}

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
    std::make_unique<FirstPersonCameraBehaviour>(glm::vec3{ 0, 2.0F, -3.0F },
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

  double last_time = glfwGetTime();
  while (!app.should_close()) {
    const double now = glfwGetTime();
    const double dt = now - last_time;
    last_time = now;
    camera_input->tick(dt);

    event_dispatcher.process_events();
    auto sc_result = context->recreate_swapchain(app.get_window().width,
                                                 app.get_window().height);

    if (sc_result == IContext::SwapchainRecreateResult::Success) {
      renderer.resize(app.get_window().width, app.get_window().height);
    }

    renderer.begin_frame(camera);
    auto& cmd = context->acquire_command_buffer();
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
