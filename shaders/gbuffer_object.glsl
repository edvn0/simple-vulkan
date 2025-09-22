#pragma stage : vertex

#include <ubo.glsl>

layout(location = 0) in vec3 v_pos;
layout(location = 1) in vec3 v_normals;
layout(location = 2) in vec2 v_uvs;

layout(location = 0) out vec3 out_world_position;
layout(location = 1) out vec3 out_world_normals;
layout(location = 2) out vec2 out_uvs;

layout(push_constant) uniform PC
{
  mat4 model;
  UboRef ubo;
  uint material_index;
  uint _pad;
}
pc;

void
main()
{
  vec4 wp = pc.model * vec4(v_pos, 1.0);
  out_world_position = wp.xyz;
  out_world_normals =
    normalize((transpose(inverse(pc.model)) * vec4(v_normals, 0.0)).xyz);
  out_uvs = v_uvs;
  gl_Position = pc.ubo.view_proj * wp;
}

#pragma stage : fragment

#include <ubo.glsl>

layout(location = 0) in vec3 out_world_position;
layout(location = 1) in vec3 out_world_normals;
layout(location = 2) in vec2 out_uvs;

layout(location = 0) out uint out_material_id_bits;   // bind to R_F32
layout(location = 1) out vec4 out_oct_normals_extras; // bind to A2R10G10B10_UN
layout(location = 2) out vec2 out_texture_coords;     // bind to RGF16

layout(push_constant) uniform PC
{
  mat4 model;
  UboRef ubo;
  uint material_index;
  uint _pad;
}
pc;

vec2
encode_oct(vec3 n)
{
  n = normalize(n);
  n /= (abs(n.x) + abs(n.y) + abs(n.z));
  vec2 e = n.z >= 0.0 ? n.xy : (1.0 - abs(n.yx)) * sign(n.xy);
  return e * 0.5 + 0.5;
}

void
main()
{
  vec2 oct = encode_oct(out_world_normals);
  out_material_id_bits = pc.material_index;
  out_oct_normals_extras = vec4(oct, 0.0, 1.0);
  out_texture_coords = out_uvs;
}
