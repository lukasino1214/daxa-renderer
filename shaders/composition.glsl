#define SETTINGS_SHADING_MODEL_GAUSSIAN
#include <shared.inl>

DAXA_USE_PUSH_CONSTANT(CompositionPush)

#include "utils/core.glsl"
#include "utils/lighting.glsl"
#include "utils/position_from_depth.glsl"

#if defined(DRAW_VERT)
layout(location = 0) out f32vec2 out_uv;

void main() {
    out_uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(out_uv * 2.0f - 1.0f, 0.0f, 1.0f);
}

#elif defined(DRAW_FRAG)

layout(location = 0) out f32vec4 out_color;

layout(location = 0) in f32vec2 in_uv;

void main() {
    f32vec4 color = sample_texture(daxa_push_constant.albedo, in_uv);
    f32vec4 normal = sample_texture(daxa_push_constant.normal, in_uv);

    vec3 ambient = vec3(0.75, 0.75, 0.75);
    
    ambient *= sample_texture(daxa_push_constant.ssao, in_uv).r;

    ambient *= color.rgb;

    f32vec4 position = f32vec4(get_world_position_from_depth(in_uv, sample_texture(daxa_push_constant.depth, in_uv).r), 1.0);
    
    f32vec3 camera_position = CAMERA.inverse_view_matrix[3].xyz;

    for(uint i = 0; i < LIGHTS.num_directional_lights; i++) {
        ambient += calculate_directional_light(LIGHTS.directional_lights[i], ambient.rgb, normal.xyz, position.xyz, camera_position);
    }

    for(uint i = 0; i < LIGHTS.num_point_lights; i++) {
        ambient += calculate_point_light(LIGHTS.point_lights[i], ambient.rgb, normal.xyz, position.xyz, camera_position);
    }

    for(uint i = 0; i < LIGHTS.num_spot_lights; i++) {
        ambient += calculate_spot_light(LIGHTS.spot_lights[i], ambient.rgb, normal.xyz, position.xyz, camera_position);
    }

    f32vec3 bloom = sample_texture(daxa_push_constant.emissive, in_uv).rgb;

    ambient += bloom * 2;

    out_color = f32vec4(ambient, 1.0);
}

#endif