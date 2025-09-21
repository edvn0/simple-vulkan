#pragma stage : vertex

#include <deferred_common.glsl>

layout(location = 0) in vec3 v_pos; // RGB32
layout(location = 1) in vec3 v_normals; // A2RGB10
layout(location = 2) in vec2 v_uvs;

layout(location = 0) out VertexOut v_out;

struct UBO
{
  mat4 view;
  mat4 projection;
  mat4 view_proj;
  mat4 inverse_view;
  mat4 inverse_projection;
  mat4 inverse_view_proj;
};

layout(buffer_reference, std430) buffer readonly UBOBuffer
{
  UBOBuffer ubo;
};

layout(push_constant) uniform PC{
  mat4 model;
  UBOBuffer buffer;
};

void
main()
{
  VertexOut vertex_out = {};

  vec4 wp = model * vec4(v_pos, 1.0);

  vertex_out.world_position = wp.xyz;
  vertex_out.world_normals = (transpose(inverse(model)) * vec4(v_normals)).xyz;
  gl_Position = buffer.ubo.view_proj * wp;
}

#pragma stage : fragment

#include <deferred_common.glsl>

layout(location = 0) in VertexOut v_in;

layout(location = 0)