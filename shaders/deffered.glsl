#define SETTINGS_SHADING_MODEL_GAUSSIAN

#include <shared.inl>

DAXA_USE_PUSH_CONSTANT(DrawPush)

#include "utils/lighting.glsl"
#include "utils/core.glsl"

#if defined(DRAW_VERT)
layout(location = 0) out f32vec2 out_uv;
layout(location = 1) out f32vec3 out_position;
layout(location = 2) out f32vec3 out_camera_position;
layout(location = 3) out f32vec3 out_normal;

void main() {
    gl_Position = CAMERA.projection_matrix * CAMERA.view_matrix * OBJECT.model_matrix * f32vec4(VERTEX_POSITION, 1.0);
    out_uv = VERTEX_UV;
    out_position = (OBJECT.model_matrix * f32vec4(VERTEX_POSITION, 1.0)).rgb;
    out_camera_position = CAMERA.position;

    out_normal = normalize(f32mat3x3(OBJECT.normal_matrix) * VERTEX_NORMAL);
}

#elif defined(DRAW_FRAG)

layout(location = 0) out f32vec4 out_albedo;
layout(location = 1) out f32vec4 out_normal;
layout(location = 2) out f32vec4 out_emissive;

layout(location = 0) in f32vec2 in_uv;
layout(location = 1) in f32vec3 in_position;
layout(location = 2) in f32vec3 in_camera_position;
layout(location = 3) in f32vec3 in_normal;

f32vec3 getNormalFromMap(TextureId normal_map) {
    f32vec3 tangent_normal = sample_texture(normal_map, in_uv).xyz * 2.0 - 1.0;

    f32vec3 Q1  = dFdx(in_position);
    f32vec3 Q2  = dFdy(in_position);
    f32vec2 st1 = dFdx(in_uv);
    f32vec2 st2 = dFdy(in_uv);

    f32vec3 N   = normalize(in_normal);

    f32vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    f32vec3 B  = normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangent_normal);
}


void main() {
    f32vec4 color = sample_texture(MATERIAL.albedo, in_uv);
    if(color.a < 0.1) { discard; }

    f32vec3 normal = normalize(in_normal);
    if(MATERIAL.has_normal_map == 1) {
        normal = normalize(getNormalFromMap(MATERIAL.normal_map));
    }

    out_normal = f32vec4(normal, 1);

    f32vec3 emissive  = f32vec3(0.0);
    if(MATERIAL.has_emissive_map == 1) {
        emissive = sample_texture(MATERIAL.emissive_map, in_uv).rgb;
    }
    out_albedo = color;
    out_albedo.rgb += emissive;

    out_emissive = f32vec4(emissive, 1);
}

#endif