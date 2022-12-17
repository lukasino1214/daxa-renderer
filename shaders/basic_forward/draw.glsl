#include <shared.inl>
#include <common/core.glsl>

DAXA_USE_PUSH_CONSTANT(DrawPush)

#define VERTEX deref(daxa_push_constant.face_buffer[gl_VertexIndex])
#define OBJECT deref(daxa_push_constant.object_buffer)
#define CAMERA deref(daxa_push_constant.camera_buffer)
#define MATERIAL deref(daxa_push_constant.material_info_buffer)

#if defined(DRAW_VERT)
layout(location = 0) out f32vec2 v_uv;
layout(location = 1) out f32vec3 v_position;
layout(location = 2) out f32vec3 v_camera_position;
#if defined(SETTINGS_NORMAL_MAPPING_USING_TANGENTS)
layout(location = 3) out f32mat3x3 v_tbn;
#else
layout(location = 3) out f32vec3 v_normal;
#endif

void main() {
    f32vec3 position = (OBJECT.model_matrix * f32vec4(VERTEX.position.xyz, 1)).xyz;
    gl_Position = CAMERA.projection_matrix * CAMERA.view_matrix * f32vec4(position.xyz, 1);

    v_uv = VERTEX.uv.xy;
#if !defined(SETTINGS_NORMAL_MAPPING_USING_TANGENTS)
    v_normal = f32mat3x3(OBJECT.normal_matrix) * VERTEX.normal.xyz;
#else
    f32vec3 normal = normalize(f32mat3x3(OBJECT.normal_matrix) * VERTEX.normal.xyz);
    f32vec3 tangent = normalize(f32mat3x3(OBJECT.normal_matrix) * VERTEX.tangent.xyz);
    
#if defined(SETTINGS_NORMAL_MAPPING_REORTHOGONALIZE_TBN_VECTORS)
    tangent = normalize(tangent - dot(tangent, normal) * normal);
#endif

    f32vec3 bittangent = normalize(cross(normal, tangent) * VERTEX.tangent.w);
    v_tbn = f32mat3x3(tangent, bittangent, normal);
#endif
    v_position = position.xyz;
    v_camera_position = CAMERA.position;
    //v_camera_position = push_constant.camera_position;
}

#elif defined(DRAW_FRAG)
layout(location = 0) in f32vec2 v_uv;
layout(location = 1) in f32vec3 v_position;
layout(location = 2) in f32vec3 v_camera_position;
#if defined(SETTINGS_NORMAL_MAPPING_USING_TANGENTS)
layout(location = 3) in f32mat3x3 v_tbn;
#else
layout(location = 3) in f32vec3 v_normal;
#endif

layout(location = 0) out f32vec4 out_color;

const f32 PI = 3.14159265359;

vec3 getNormalFromMap(TextureId normal_map) {
    vec3 tangentNormal = sample_texture(normal_map, v_uv).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(v_position);
    vec3 Q2  = dFdy(v_position);
    vec2 st1 = dFdx(v_uv);
    vec2 st2 = dFdy(v_uv);

#if defined(SETTINGS_NORMAL_MAPPING_USING_TANGENTS)
    vec3 N   = normalize(v_tbn[2]);
#else
    vec3 N   = normalize(v_normal);
#endif
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

void main() {
#if defined(SETTINGS_TEXTURING_NONE)
    f32vec3 color = f32vec3(1.0, 1.0, 1.0);
#elif defined(SETTINGS_TEXTURING_VERTEX_COLOR)
    f32vec3 color = f32vec3(0.5, 0.5, 0.5);
#elif defined(SETTINGS_TEXTURING_ALBEDO)
    f32vec3 color = sample_texture(MATERIAL.albedo, v_uv).rgb;
#else
    f32vec3 color = f32vec3(0.0, 0.0, 0.0);
#endif

#if !defined(SETTINGS_SHADING_MODEL_NONE)
#if defined(SETTINGS_NORMAL_MAPPING_NONE)
    f32vec3 normal = normalize(v_normal);
#elif defined(SETTINGS_NORMAL_MAPPING_CALCULATING_TBN_VECTORS)
    f32vec3 normal = getNormalFromMap(MATERIAL.normal_map);
#elif defined(SETTINGS_NORMAL_MAPPING_USING_TANGENTS)
    f32vec3 normal = normalize(v_tbn * normalize(sample_texture(MATERIAL.normal_map, v_uv).rgb * 2.0 - 1.0));
#endif
    for(uint i = 0; i < deref(daxa_push_constant.lights_buffer).num_directional_lights; i++) {
        color += calculate_directional_light(deref(daxa_push_constant.lights_buffer).directional_lights[i], color, normal, v_position, v_camera_position);
    }

    for(uint i = 0; i < deref(daxa_push_constant.lights_buffer).num_point_lights; i++) {
        color += calculate_point_light(deref(daxa_push_constant.lights_buffer).point_lights[i], color, normal, v_position, v_camera_position);
    }

    for(uint i = 0; i < deref(daxa_push_constant.lights_buffer).num_spot_lights; i++) {
        color += calculate_spot_light(deref(daxa_push_constant.lights_buffer).spot_lights[i], color, normal, v_position, v_camera_position);
    }
    #endif

    out_color = vec4(color, 1.0);
}

#endif