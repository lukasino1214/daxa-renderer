#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#define SETTINGS_SHADING_MODEL_GAUSSIAN
#include <shared.inl>

#include "lighting.glsl"

DAXA_USE_PUSH_CONSTANT(CompositionPush)

#define VERTEX deref(daxa_push_constant.face_buffer[gl_VertexIndex])
#define OBJECT deref(daxa_push_constant.object_buffer)
#define CAMERA deref(daxa_push_constant.camera_buffer)
#define MATERIAL deref(daxa_push_constant.material_info_buffer)
#define LIGHTS deref(daxa_push_constant.lights_buffer)

#define sample_texture(tex, uv) texture(tex.image_view_id, tex.sampler_id, uv)

#if defined(DRAW_VERT)
layout(location = 0) out f32vec2 out_uv;

void main() {
    out_uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(out_uv * 2.0f - 1.0f, 0.0f, 1.0f);
}

#elif defined(DRAW_FRAG)

layout(location = 0) out f32vec4 out_color;

layout(location = 0) in f32vec2 in_uv;

vec3 get_world_position_from_depth(vec2 uv, float depth) {
    vec4 clipSpacePosition = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewSpacePosition = CAMERA.inverse_projection_matrix * clipSpacePosition;

    viewSpacePosition /= viewSpacePosition.w;
    vec4 worldSpacePosition = CAMERA.inverse_view_matrix * viewSpacePosition;

    return worldSpacePosition.xyz;
}

vec3 get_view_position_from_depth(vec2 uv, float depth) {
    vec4 clipSpacePosition = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewSpacePosition = CAMERA.inverse_projection_matrix * clipSpacePosition;

    viewSpacePosition /= viewSpacePosition.w;

    return viewSpacePosition.xyz;
}

void main() {
    f32vec4 color = sample_texture(daxa_push_constant.albedo, in_uv);
    f32vec4 normal = sample_texture(daxa_push_constant.normal, in_uv);

    vec3 ambient = vec3(0.75, 0.75, 0.75);
    
    ambient *= sample_texture(daxa_push_constant.ssao, in_uv).r;

    ambient *= color.rgb;

    f32vec4 position = f32vec4(get_world_position_from_depth(in_uv, sample_texture(daxa_push_constant.depth, in_uv).r), 1.0);
    
    f32vec3 camera_position = CAMERA.inverse_view_matrix[3].xyz;

    for(uint i = 0; i < LIGHTS.num_directional_lights; i++) {
        ambient += calculate_directional_light(LIGHTS.directional_lights[i], color.rgb, normal.xyz, position.xyz, camera_position);
    }

    for(uint i = 0; i < LIGHTS.num_point_lights; i++) {
        ambient += calculate_point_light(LIGHTS.point_lights[i], color.rgb, normal.xyz, position.xyz, camera_position);
    }

    for(uint i = 0; i < LIGHTS.num_spot_lights; i++) {
        ambient += calculate_spot_light(LIGHTS.spot_lights[i], color.rgb, normal.xyz, position.xyz, camera_position);
    }

    out_color = f32vec4(ambient, 1.0);
}

#endif