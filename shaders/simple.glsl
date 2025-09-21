#pragma stage : vertex
layout(location = 0) out vec3 v_color;
layout(location = 1) out vec2 v_uv;

layout(push_constant) uniform PC
{
  float time;
  uint tex;
  mat4 ortho_mvp;
}
pc;

vec2 positions[3] = vec2[](vec2(0.0, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5));
vec3 colors[3] =
  vec3[](vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0));
vec2 uvs[3] = vec2[](vec2(0.5, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));

void
main()
{
  float a = pc.time;
  mat2 r = mat2(cos(a), -sin(a), sin(a), cos(a));
  vec2 p = r * positions[gl_VertexIndex];
  gl_Position = pc.ortho_mvp * vec4(p, 0.0, 1.0);
  v_color = colors[gl_VertexIndex];
  v_uv = uvs[gl_VertexIndex];
}

#pragma stage : fragment
layout(location = 0) in vec3 v_color;
layout(location = 1) in vec2 v_uv;
layout(location = 0) out vec4 colour;

layout(push_constant) uniform PC
{
  float time;
  uint tex;
  mat4 ortho_mvp;
}
pc;

void
main()
{
  vec4 s = textureBindless2D(pc.tex, 0, v_uv);
  colour = vec4(v_color * s.rgb, 1.0);
}
