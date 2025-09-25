

#pragma stage : vertex
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec4 in_tex_coords;
layout(location = 2) in vec4 in_normals;
layout(location = 3) in vec4 in_tangents;
layout(location = 4) in vec4 in_bitangents;

struct Cascade
{
  mat4 view;
  mat4 proj;
  mat4 vp;
};
layout(buffer_reference) readonly buffer ShadowUboRef
{
  Cascade cascades[8];
  uint cascade_count;
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

layout(push_constant) uniform PC
{
  ShadowUboRef ubo;
  InstancesRef instances;
  uint cascade_index;
  uint _pad;
}
pc;

void
main()
{
  uint idx = gl_BaseInstance + gl_InstanceIndex;
  InstanceData d = pc.instances.data[idx];
  vec4 wp = d.model * vec4(in_pos, 1.0);
  gl_Position = pc.ubo.cascades[pc.cascade_index].vp * wp;
}

#pragma stage : fragment
void
main()
{
}
