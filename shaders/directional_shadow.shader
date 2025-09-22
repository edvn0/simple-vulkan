

#pragma stage : vertex
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(location = 0) out vec3 v_pos_ws;

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

layout(push_constant) uniform PushConstants
{
  ShadowUboRef ubo; // device address to cascades UBO
  mat4 model;
}
pc;

void
main()
{
  v_pos_ws = vec3(pc.model * vec4(in_pos, 1.0));
}

#pragma stage : geometry
layout(triangles) in;
layout(triangle_strip, max_vertices = 12) out;

layout(location = 0) in vec3 v_pos_ws_in[3];
layout(location = 0) out vec3 v_pos_ws_out;

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

layout(push_constant) uniform PushConstants
{
  ShadowUboRef ubo; // device address to cascades UBO
  mat4 model;
}
pc;

void
main()
{
  uint count = min(pc.ubo.cascade_count, 4u);

  for (uint c = 0; c < count; ++c) {
    gl_Layer = int(c);
    for (int i = 0; i < 3; ++i) {
      v_pos_ws_out = v_pos_ws_in[i];
      gl_Position = pc.ubo.cascades[c].vp * vec4(v_pos_ws_out, 1.0);
      EmitVertex();
    }
    EndPrimitive();
  }
}

#pragma stage : fragment
void
main()
{
}
