#pragma stage : vertex

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_uv;
struct Vertex
{
  vec4 pos;
  vec4 rgba;
};
layout(std430, buffer_reference) readonly buffer VertexBuffer
{
  Vertex vertices[];
};
layout(push_constant) uniform PushConstants
{
  mat4 mvp;
  VertexBuffer vb;
};

void
main()
{
  Vertex v = vb.vertices[gl_VertexIndex];
  out_color = v.rgba;
  gl_Position = mvp * v.pos;
}

#pragma stage : fragment

layout(location = 0) in vec4 in_color;
layout(location = 0) out vec4 out_color;

void
main()
{
  out_color = in_color;
}