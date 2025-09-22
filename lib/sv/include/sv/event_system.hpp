#pragma once

#include <algorithm>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

extern "C"
{
  struct GLFWwindow;
}

namespace sv::EventSystem {

template<typename T>
struct EventTypeId
{
  static constexpr auto id() -> std::uint32_t
  {
    return reinterpret_cast<std::uintptr_t>(&EventTypeId::id) & 0xFFFFFFFF;
  }
};

struct IEvent
{
  virtual ~IEvent() = default;
  virtual auto get_type_id() const -> std::uint32_t = 0;

  mutable bool consumed = false;
};

template<typename Derived>
struct Event : IEvent
{
  auto get_type_id() const -> std::uint32_t override
  {
    return EventTypeId<Derived>::id();
  }
};

// Event types
struct KeyEvent final : Event<KeyEvent>
{
  std::int32_t key{};
  std::int32_t scancode{};
  std::int32_t action{};
  std::int32_t mods{};
};

struct MouseButtonEvent final : Event<MouseButtonEvent>
{
  std::int32_t button{};
  std::int32_t action{};
  std::int32_t mods{};
};

struct MouseMoveEvent final : Event<MouseMoveEvent>
{
  double x_pos{};
  double y_pos{};
  double delta_x{};
  double delta_y{};
};

struct WindowResizeEvent final : Event<WindowResizeEvent>
{
  std::int32_t width{};
  std::int32_t height{};
};

struct FramebufferSizeEvent final : Event<FramebufferSizeEvent>
{
  std::int32_t width{};
  std::int32_t height{};
};

class IEventHandler
{
public:
  virtual ~IEventHandler() = default;

  virtual auto on_event(const IEvent& event) -> bool = 0;

  [[nodiscard]] virtual auto get_priority() const -> std::int32_t { return 0; }
};

template<typename... EventTypes>
class TypedEventHandler : public IEventHandler
{
public:
  auto on_event(const IEvent& event) -> bool override
  {
    return dispatch_event(event);
  }

protected:
  virtual bool handle_event(const KeyEvent&) { return false; }
  virtual bool handle_event(const FramebufferSizeEvent&) { return false; }
  virtual bool handle_event(const MouseButtonEvent&) { return false; }
  virtual bool handle_event(const MouseMoveEvent&) { return false; }
  virtual bool handle_event(const WindowResizeEvent&) { return false; }

private:
  template<typename K>
  static auto safe_dynamic_cast(const IEvent& event) -> const K&
  {
#ifdef IS_DEBUG
    if (event.get_type_id() != EventTypeId<K>::id()) {
      throw std::exception("Invalid event type cast");
    }
    return dynamic_cast<const K&>(event);
#else
    return static_cast<const K&>(event);
#endif
  }

  auto dispatch_event(const IEvent& event) -> bool
  {
    const auto event_type = event.get_type_id();

    if constexpr (has_type<KeyEvent, EventTypes...>()) {
      if (event_type == EventTypeId<KeyEvent>::id()) {
        return handle_event(safe_dynamic_cast<const KeyEvent&>(event));
      }
    }
    if constexpr (has_type<MouseButtonEvent, EventTypes...>()) {
      if (event_type == EventTypeId<MouseButtonEvent>::id()) {
        return handle_event(safe_dynamic_cast<const MouseButtonEvent&>(event));
      }
    }
    if constexpr (has_type<MouseMoveEvent, EventTypes...>()) {
      if (event_type == EventTypeId<MouseMoveEvent>::id()) {
        return handle_event(safe_dynamic_cast<const MouseMoveEvent&>(event));
      }
    }
    if constexpr (has_type<WindowResizeEvent, EventTypes...>()) {
      if (event_type == EventTypeId<WindowResizeEvent>::id()) {
        return handle_event(safe_dynamic_cast<const WindowResizeEvent&>(event));
      }
    }

    return false;
  }

  template<typename T, typename... Types>
  static constexpr bool has_type()
  {
    return (std::is_same_v<T, Types> || ...);
  }
};

using EventHandler = TypedEventHandler<KeyEvent,
                                       MouseButtonEvent,
                                       MouseMoveEvent,
                                       WindowResizeEvent>;

class EventDispatcher
{
  struct HandlerInfo
  {
    std::weak_ptr<IEventHandler> handler;
    std::int32_t priority;

    auto operator<=>(const HandlerInfo& other) const
    {
      return other.priority <=> priority;
    }
  };

  std::unordered_map<std::uint32_t, std::vector<HandlerInfo>> event_handlers;

  double last_mouse_x = 0.0;
  double last_mouse_y = 0.0;
  bool mouse_initialised = false;

public:
  template<typename EventType>
  auto subscribe(const std::shared_ptr<IEventHandler>& handler) -> void
  {
    const auto type_id = EventTypeId<EventType>::id();
    event_handlers[type_id].emplace_back(handler, handler->get_priority());

    // Sort by priority after adding
    auto& handler_list = event_handlers.at(type_id);
    std::sort(std::begin(handler_list), std::end(handler_list));
  }

  template<typename... EventTypes>
    requires(sizeof...(EventTypes) > 1)
  auto subscribe(const std::shared_ptr<IEventHandler>& handler) -> void
  {
    (subscribe<EventTypes>(handler), ...);
  }

  template<typename EventType>
  auto unsubscribe(std::shared_ptr<IEventHandler> handler) -> void
  {
    const auto type_id = EventTypeId<EventType>::id();
    auto& handler_list = event_handlers.at(type_id);

    handler_list.erase(std::remove_if(handler_list.begin(),
                                      handler_list.end(),
                                      [&handler](const HandlerInfo& info) {
                                        return info.handler.expired() ||
                                               info.handler.lock() == handler;
                                      }),
                       handler_list.end());
  }

  // Dispatch event to all registered handlers
  auto dispatch(const IEvent& event) -> void
  {
    const auto type_id = event.get_type_id();
    const auto it = event_handlers.find(type_id);
    if (it == event_handlers.end()) {
      return;
    }

    auto& handler_list = it->second;

    // Clean up expired handlers while processing
    for (auto handler_it = handler_list.begin();
         handler_it != handler_list.end();) {
      if (handler_it->handler.expired()) {
        handler_it = handler_list.erase(handler_it);
        continue;
      }

      if (const auto handler = handler_it->handler.lock();
          handler->on_event(event)) {
        break;
      }

      if (event.consumed) {
        break;
      }

      ++handler_it;
    }
  }

  auto handle_key_callback(GLFWwindow*,
                           const std::int32_t key,
                           const std::int32_t scancode,
                           const std::int32_t action,
                           const std::int32_t mods) -> void
  {
    KeyEvent event;
    event.key = key;
    event.scancode = scancode;
    event.action = action;
    event.mods = mods;
    event.consumed = false;
    dispatch(event);
  }

  auto handle_mouse_button_callback(GLFWwindow*,
                                    const std::int32_t button,
                                    const std::int32_t action,
                                    const std::int32_t mods) -> void
  {
    MouseButtonEvent event;
    event.button = button;
    event.action = action;
    event.mods = mods;
    event.consumed = false;
    dispatch(event);
  }

  auto handle_cursor_pos_callback(GLFWwindow*,
                                  const double x_pos,
                                  const double y_pos) -> void
  {
    double delta_x = 0.0;
    double delta_y = 0.0;

    if (mouse_initialised) {
      delta_x = x_pos - last_mouse_x;
      delta_y = y_pos - last_mouse_y;
    } else {
      mouse_initialised = true;
    }

    last_mouse_x = x_pos;
    last_mouse_y = y_pos;

    MouseMoveEvent event;
    event.x_pos = x_pos;
    event.y_pos = y_pos;
    event.delta_x = delta_x;
    event.delta_y = delta_y;
    event.consumed = false; // Reset consumed state for each event
    dispatch(event);
  }

  auto handle_window_size_callback(GLFWwindow*,
                                   const std::int32_t width,
                                   const std::int32_t height) -> void
  {
    WindowResizeEvent event;
    event.width = width;
    event.height = height;
    event.consumed = false;
    dispatch(event);
  }

  auto handle_framebuffer_size_callback(GLFWwindow*,
                                        const std::int32_t width,
                                        const std::int32_t height) -> void
  {
    FramebufferSizeEvent event;
    event.width = width;
    event.height = height;
    event.consumed = false;
    dispatch(event);
  }

  auto process_events() -> void;
};

} // namespace EventSystem