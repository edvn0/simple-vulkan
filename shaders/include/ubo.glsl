#ifndef UBO_GLSL
#define UBO_GLSL

struct UboData
{
  mat4 view;
  mat4 projection;
  mat4 view_proj;
  mat4 inverse_view;
  mat4 inverse_projection;
  mat4 inverse_view_proj;
  vec4 light_direction;
  vec4 camera_position;
};

layout(buffer_reference, std430) readonly buffer UboRef
{
  UboData u;
};

struct InstanceData
{
  mat4 model;
  uint material_index;
  uint pad0, pad1, pad2;
};

layout(buffer_reference, std430) readonly buffer InstancesRef
{
  InstanceData data[];
};

#endif