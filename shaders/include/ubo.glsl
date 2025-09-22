#ifndef UBO_GLSL
#define UBO_GLSL

layout(buffer_reference, std430) readonly buffer UboRef
{
  mat4 view;
  mat4 projection;
  mat4 view_proj;
  mat4 inverse_view;
  mat4 inverse_projection;
  mat4 inverse_view_proj;
  vec4 light_direction;
  vec4 camera_position;
}
ubo;

#endif