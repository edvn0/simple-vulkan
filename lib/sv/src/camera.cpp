#include "sv/camera.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

namespace sv {

auto
FirstPersonCameraBehaviour::update(double dt,
                                   const glm::vec2& new_mouse_pos,
                                   bool mouse_pressed) -> void
{
  if (mouse_pressed) {
    auto delta{ mouse_position - new_mouse_pos };
    auto delta_quat =
      glm::quat(glm::vec3(mouse_speed * delta.y, -mouse_speed * delta.x, 0.0f));
    camera_orientation = glm::normalize(delta_quat * camera_orientation);

    set_up_vector(up_vector);
  }

  mouse_position = new_mouse_pos;

  const auto forward = glm::normalize(camera_orientation * glm::vec3(0, 0, 1));
  const auto right = glm::normalize(glm::cross(up_vector, forward));
  const auto up = glm::normalize(glm::cross(forward, right));

  glm::vec3 accel(0.0f);
  if (movement.forward)
    accel += forward;
  if (movement.backward)
    accel -= forward;
  if (movement.left)
    accel -= right;
  if (movement.right)
    accel += right;
  if (movement.up)
    accel += up;
  if (movement.down)
    accel -= up;
  if (movement.fast_speed)
    accel *= fast_speed_factor;

  if (glm::all(glm::epsilonEqual(accel, glm::vec3(0.0f), 0.001f))) {
    move_speed -=
      move_speed * std::min((1.0f / damping) * static_cast<float>(dt), 1.0f);

  } else {
    move_speed += accel * acceleration * static_cast<float>(dt);
    const float maximum_speed =
      movement.fast_speed ? max_speed * fast_speed_factor : max_speed;
    if (glm::length(move_speed) > maximum_speed)
      move_speed = glm::normalize(move_speed) * maximum_speed;
  }
  camera_position += move_speed * static_cast<float>(dt);
}

auto
FirstPersonCameraBehaviour::get_view_matrix() const -> glm::mat4
{
  const auto r_inv = glm::mat4_cast(glm::conjugate(camera_orientation));
  const auto t_inv = glm::translate(glm::mat4(1.0f), -camera_position);
  return r_inv * t_inv;
}

auto
FirstPersonCameraBehaviour::set_up_vector(const glm::vec3& up) -> void
{
  up_vector = glm::normalize(up);
  const auto f = glm::normalize(camera_orientation * glm::vec3(0, 0, 1));
  auto r = glm::cross(up_vector, f);
  if (glm::length2(r) < 1e-6f)
    r = glm::vec3(1, 0, 0);
  r = glm::normalize(r);
  const auto u = glm::normalize(glm::cross(f, r));
  const glm::mat3 basis{ r, u, f };
  camera_orientation = glm::normalize(glm::quat_cast(basis));
}

}