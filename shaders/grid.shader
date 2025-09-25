#pragma stage : vertex

#include <ubo.glsl>

layout(push_constant) uniform PushConstants
{
  UboRef ubo_ref;
  vec4 origin;
  vec4 grid_colour_thin;
  vec4 grid_colour_thick;
  vec4 grid_params;
};

layout(location = 0) out vec2 uv;
layout(location = 1) out vec2 out_camPos;

const vec3 pos[4] = vec3[4](vec3(-1.0, 0.0, -1.0),
                            vec3(1.0, 0.0, -1.0),
                            vec3(1.0, 0.0, 1.0),
                            vec3(-1.0, 0.0, 1.0));
const int indices[6] = int[6](0, 1, 2, 2, 3, 0);

void
main()
{
  int idx = indices[gl_VertexIndex];
  vec3 position = pos[idx] * grid_params.x; // grid_size
  position.x += ubo_ref.u.camera_position.x;
  position.z += ubo_ref.u.camera_position.z;
  position += origin.xyz;

  out_camPos = ubo_ref.u.camera_position.xz;
  gl_Position = ubo_ref.u.projection * ubo_ref.u.view * vec4(position, 1.0);
  uv = position.xz;
}

#pragma stage : fragment

#include <math_helpers.glsl>
#include <ubo.glsl>

layout(push_constant) uniform PushConstants
{
  UboRef ubo_ref;
  vec4 origin;
  vec4 grid_colour_thin;
  vec4 grid_colour_thick;
  vec4 grid_params;
};

layout(location = 0) in vec2 uv;
layout(location = 1) in vec2 out_camPos;

layout(location = 0) out vec4 frag_colour;

vec4
gridColor(vec2 uv, vec2 camPos)
{
  vec2 dudv = vec2(length(vec2(dFdx(uv.x), dFdy(uv.x))),
                   length(vec2(dFdx(uv.y), dFdy(uv.y))));

  float lodLevel =
    max(0.0, log10((length(dudv) * grid_params.z) / grid_params.y) + 1.0);
  float lodFade = fract(lodLevel);

  float lod0 = grid_params.y * pow(10.0, floor(lodLevel));
  float lod1 = lod0 * 10.0;
  float lod2 = lod1 * 10.0;

  dudv *= 4.0;
  uv += dudv * 0.5;

  float lod0a =
    max2(vec2(1.0) - abs(satv(mod(uv, lod0) / dudv) * 2.0 - vec2(1.0)));
  float lod1a =
    max2(vec2(1.0) - abs(satv(mod(uv, lod1) / dudv) * 2.0 - vec2(1.0)));
  float lod2a =
    max2(vec2(1.0) - abs(satv(mod(uv, lod2) / dudv) * 2.0 - vec2(1.0)));

  vec4 c = lod2a > 0.0   ? grid_colour_thick
           : lod1a > 0.0 ? mix(grid_colour_thick, grid_colour_thin, lodFade)
                         : grid_colour_thin;

  uv -= camPos;
  float opacityFalloff = (1.0 - satf(length(uv) / grid_params.x)); // grid_size
  c.a *= lod2a > 0.0 ? lod2a : lod1a > 0.0 ? lod1a : (lod0a * (1.0 - lodFade));
  c.a *= opacityFalloff;

  return c;
}

void
main()
{
  vec4 c = gridColor(uv, out_camPos);
  if (c.a < 0.01)
    discard;       // let background show through
  frag_colour = c; // self-lit
}
