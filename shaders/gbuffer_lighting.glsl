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
layout(location = 0) out vec4 o_hdr;

layout(push_constant) uniform pc_lighting
{
  uint normals_tex;
  uint depth_tex;
  uint material_tex;
  uint uvs_tex;
  uint sampler_id;
}
pc;

vec3
decode_oct(vec2 e)
{
  vec2 f = e * 2.0 - 1.0;
  vec3 n = vec3(f, 1.0 - abs(f.x) - abs(f.y));
  if (n.z < 0.0)
    n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
  return normalize(n);
}

void
main()
{
  vec2 uvs = textureBindless2D(pc.uvs_tex, pc.sampler_id, v_uv).rg;
  vec4 ne = textureBindless2D(pc.normals_tex, pc.sampler_id, v_uv);
  float depth = textureBindless2D(pc.depth_tex, pc.sampler_id, v_uv).r;
  uint mat_id =
    floatBitsToUint(textureBindless2D(pc.material_tex, pc.sampler_id, v_uv).r);

  vec3 n = decode_oct(ne.rg);
  vec3 l = normalize(vec3(0.4, 0.8, 0.2));
  float ndotl = max(dot(n, l), 0.0);

  float hue = fract(sin(float(mat_id) * 12.9898) * 43758.5453);
  vec3 base =
    clamp(abs(fract(hue + vec3(0.0, 0.33, 0.66)) * 3.0 - 1.5) * 1.1, 0.0, 1.0);

  float depth_attn = clamp(depth, 0.0, 1.0);
  vec3 color = base * (0.05 + 0.95 * ndotl) * mix(0.25, 1.0, depth_attn);

  o_hdr = vec4(color, 1.0);
}