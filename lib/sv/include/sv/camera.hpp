#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <memory>

namespace sv {

struct CameraBehaviour
{
  virtual ~CameraBehaviour() = default;

  [[nodiscard]] virtual auto get_view_matrix() const -> glm::mat4 = 0;
  [[nodiscard]] virtual auto get_position() const -> glm::vec3 = 0;
};

class Camera final
{
  std::unique_ptr<CameraBehaviour> camera_behaviour;

public:
  explicit Camera(std::unique_ptr<CameraBehaviour> cam)
    : camera_behaviour(std::move(cam))
  {
  }
  [[nodiscard]] auto get_view_matrix() const
  {
    return camera_behaviour->get_view_matrix();
  }

  [[nodiscard]] auto get_position() const
  {
    return camera_behaviour->get_position();
  }

  [[nodiscard]] auto get_behaviour() const -> CameraBehaviour*
  {
    return camera_behaviour.get();
  }
};

struct FirstPersonCameraBehaviour final : CameraBehaviour
{
  struct Movement
  {
    bool forward{ false };
    bool backward{ false };
    bool left{ false };
    bool right{ false };
    bool up{ false };
    bool down{ false };
    bool fast_speed{ false };
  };

  Movement movement{};
  float mouse_speed{ 4.0F };
  float acceleration{ 150.0F };
  float damping{ 0.2F };
  float max_speed{ 10.F };
  float fast_speed_factor{ 10.0F };

  glm::vec2 mouse_position = glm::vec2{ 0 };
  glm::vec3 camera_position = { 0.0f, 10.0f, 10.0f };
  glm::quat camera_orientation = glm::quat(glm::vec3(0));
  glm::vec3 move_speed = glm::vec3{ 0.0f };
  glm::vec3 up_vector = { 0.0f, 0.0f, 1.0f };

  explicit FirstPersonCameraBehaviour(const glm::vec3& position,
                                      const glm::vec3& target,
                                      const glm::vec3& up = { 0.0f,
                                                              0.0f,
                                                              1.0f })
    : camera_position(position)
    , camera_orientation(
        glm::quatLookAtLH(glm::normalize(target - position), up))
    , up_vector(up)
  {
  }

  auto update(double dt, const glm::vec2&, bool mouse_pressed) -> void;
  auto set_up_vector(const glm::vec3&) -> void;
  [[nodiscard]] auto get_view_matrix() const -> glm::mat4 override;
  [[nodiscard]] auto get_position() const -> glm::vec3 override
  {
    return camera_position;
  }
};

}