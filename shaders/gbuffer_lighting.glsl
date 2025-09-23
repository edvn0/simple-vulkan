#pragma stage : vertex

layout(location = 0) out vec2 v_uv;
void
main()
{
  vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  v_uv = vec2(p.x, 1.0 - p.y);
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}

#pragma stage : fragment

#include <ubo.glsl>

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_hdr;

layout(push_constant) uniform pc_lighting
{
  uint normals_tex;
  uint depth_tex;
  uint material_tex;
  uint uvs_tex;
  uint sampler_id;

  uint shadow_tex;
  uint shadow_sampler_id;
  uint shadow_cascade_count;
  UboRef ubo;
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

vec3
reconstruct_world_position(float depth, vec2 uv)
{
  vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
  vec4 wpos = pc.ubo.inverse_view_proj * ndc;
  return wpos.xyz / wpos.w;
}

void
main()
{
  vec2 uvs = textureBindless2D(pc.uvs_tex, pc.sampler_id, v_uv).rg;
  vec4 ne = textureBindless2D(pc.normals_tex, pc.sampler_id, v_uv);
  float depth = textureBindless2D(pc.depth_tex, pc.sampler_id, v_uv).r;
  uint mat_id =
    floatBitsToUint(textureBindless2D(pc.material_tex, pc.sampler_id, v_uv).r);

  float ref_depth = 0.5;
  float shadow_depth =
    textureBindless2D(pc.shadow_tex, pc.shadow_sampler_id, v_uv).r;

  vec3 n = decode_oct(ne.rg);
  vec3 l = normalize(pc.ubo.light_direction.xyz);

  vec3 wpos = reconstruct_world_position(depth, v_uv);
  vec3 v = normalize(pc.ubo.camera_position.xyz - wpos);
  vec3 h = normalize(l + v);

  float ndotl = max(dot(n, l), 0.0);
  float ndoth = max(dot(n, h), 0.0);

  const float ambient_strength = 0.04;
  const float specular_intensity = 1.0;
  const float shininess = 64.0;

  float hue = fract(sin(float(mat_id) * 12.9898) * 43758.5453);
  vec3 base =
    clamp(abs(fract(hue + vec3(0.0, 0.33, 0.66)) * 3.0 - 1.5) * 1.1, 0.0, 1.0);

  vec3 light_color = vec3(0.9, 0.7, 0.1);
  vec3 ambient = ambient_strength * base;
  vec3 diffuse = shadow_depth * ndotl * base * light_color;
  vec3 specular = specular_intensity * pow(ndoth, shininess) * light_color;

  o_hdr = vec4(ambient + diffuse + specular, 1.0);
}
