#include "sv/event_system.hpp"

#include <GLFW/glfw3.h>

namespace sv::EventSystem {

auto
EventDispatcher::process_events() -> void
{
  glfwPollEvents();
}

}