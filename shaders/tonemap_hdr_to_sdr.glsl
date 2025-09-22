#pragma stage : vertex
layout(location = 0) out vec2 v_uv;
void
main()
{
  vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  v_uv = p;
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}

#pragma stage : fragment
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_color;

layout(push_constant) uniform PCTonemap
{
  uint hdr_tex;
  uint sampler_id;
  float exposure;
  float _pad;
}
pc;

vec3
reinhard(vec3 c)
{
  return c / (1.0 + c);
}

void
main()
{
  vec3 hdr =
    textureBindless2D(pc.hdr_tex, pc.sampler_id, v_uv).rgb * pc.exposure;
  vec3 ldr = pow(reinhard(hdr), vec3(1.0 / 2.2));
  o_color = vec4(ldr, 1.0);
}