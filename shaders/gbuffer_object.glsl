#pragma stage : vertex

#include <ubo.glsl>

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec4 in_tex_coords; // R16G16B16A16_SFLOAT
layout(location = 2) in vec4 in_normals;    // A2B10G10R10_SNORM_PACK32
layout(location = 3) in vec4 in_tangents;   // A2B10G10R10_SNORM_PACK32
layout(location = 4) in vec4 in_bitangents; // A2B10G10R10_UNORM_PACK32

layout(location = 0) out vec3 v_world_pos;
layout(location = 1) out vec3 v_world_nrm;
layout(location = 2) out vec2 v_uv;
layout(location = 3) out flat uint v_material_index;

layout(push_constant) uniform PC
{
  UboRef ubo;
  InstancesRef instances;
}
pc;

void
main()
{
  uint idx = gl_BaseInstance + gl_InstanceIndex;
  InstanceData d = pc.instances.data[idx];

  vec4 wp = d.model * vec4(in_pos, 1.0);
  v_world_pos = wp.xyz;

  mat3 nrm_m = mat3(transpose(inverse(d.model)));
  v_world_nrm = normalize(nrm_m * in_normals.xyz);

  v_uv = in_tex_coords.xy;
  v_material_index = d.material_index;

  gl_Position = pc.ubo.u.view_proj * wp;
}

#pragma stage : fragment

#include <ubo.glsl>

layout(location = 0) in vec3 v_world_pos;
layout(location = 1) in vec3 v_world_nrm;
layout(location = 2) in vec2 v_uv;
layout(location = 3) in flat uint v_material_index;

layout(location = 0) out uint out_material_id_bits;   // R_UI32
layout(location = 1) out vec4 out_oct_normals_extras; // A2R10G10B10_UN
layout(location = 2) out vec2 out_texture_coords;     // RG_F16

layout(push_constant) uniform PC
{
  UboRef ubo;
  InstancesRef instances;
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
  vec2 oct = encode_oct(v_world_nrm);
  out_material_id_bits = v_material_index;
  out_oct_normals_extras = vec4(oct, 0.0, 1.0);
  out_texture_coords = v_uv;
}
