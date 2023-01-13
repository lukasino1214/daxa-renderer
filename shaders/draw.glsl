#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#define SETTINGS_SHADING_MODEL_GAUSSIAN
#include <shared.inl>

#include "lighting.glsl"


DAXA_USE_PUSH_CONSTANT(DrawPush)

#define VERTEX deref(daxa_push_constant.face_buffer[gl_VertexIndex])
#define OBJECT deref(daxa_push_constant.object_buffer)
#define CAMERA deref(daxa_push_constant.camera_buffer)
#define MATERIAL deref(daxa_push_constant.material_info_buffer)
#define LIGHTS deref(daxa_push_constant.lights_buffer)

#define sample_texture(tex, uv) texture(tex.image_view_id, tex.sampler_id, uv)

#if defined(DRAW_VERT)
layout(location = 0) out f32vec2 out_uv;
layout(location = 1) out f32vec3 out_position;
layout(location = 2) out f32vec3 out_camera_position;
layout(location = 3) out f32mat3x3 out_tbn;
layout(location = 6) out f32vec3 out_normal;

void main() {
    gl_Position = CAMERA.projection_matrix * CAMERA.view_matrix * OBJECT.model_matrix * vec4(VERTEX.position.xyz, 1.0);
    //out_normal = normalize(mat3x3(OBJECT.normal_matrix) * VERTEX.normal);
    out_uv = VERTEX.uv;
    out_position = (OBJECT.model_matrix * vec4(VERTEX.position.xyz, 1.0)).rgb;
    out_camera_position = CAMERA.position;

    f32vec3 normal = normalize(f32mat3x3(OBJECT.normal_matrix) * VERTEX.normal.xyz);
    f32vec3 tangent = normalize(f32mat3x3(OBJECT.normal_matrix) * VERTEX.tangent.xyz);
    f32vec3 bittangent = normalize(cross(normal, tangent) * VERTEX.tangent.w);
    out_tbn = f32mat3x3(tangent, bittangent, normal);
    out_normal = normal;
}

#elif defined(DRAW_FRAG)

layout(location = 0) out f32vec4 out_color;

layout(location = 0) in f32vec2 in_uv;
layout(location = 1) in f32vec3 in_position;
layout(location = 2) in f32vec3 in_camera_position;
layout(location = 3) in f32mat3x3 in_tbn;
layout(location = 6) in f32vec3 in_normal;

void main() {
    f32vec4 color = sample_texture(MATERIAL.albedo, in_uv);
    if(color.a < 0.1) { discard; }

    f32vec3 normal = normalize(in_tbn * normalize(sample_texture(MATERIAL.normal_map, in_uv).rgb * 2.0 - 1.0));
    //f32vec3 normal = in_normal;

    for(uint i = 0; i < LIGHTS.num_directional_lights; i++) {
        color.rgb += calculate_directional_light(LIGHTS.directional_lights[i], color.rgb, normal, in_position, in_camera_position);
    }

    for(uint i = 0; i < LIGHTS.num_point_lights; i++) {
        color.rgb += calculate_point_light(LIGHTS.point_lights[i], color.rgb, normal, in_position, in_camera_position);
    }

    for(uint i = 0; i < LIGHTS.num_spot_lights; i++) {
        color.rgb += calculate_spot_light(LIGHTS.spot_lights[i], color.rgb, normal, in_position, in_camera_position);
    }

    //f32vec3 rgb_normal = normal * 0.5 + 0.5;

    out_color = f32vec4(color.rgb, 1.0);
}

#endif