#include <shared.inl>
#include <common/core.glsl>

DAXA_USE_PUSH_CONSTANT(CompositionPush)

#define VERTEX deref(daxa_push_constant.face_buffer[gl_VertexIndex])
#define OBJECT deref(daxa_push_constant.object_buffer)
#define CAMERA deref(daxa_push_constant.camera_buffer)
#define MATERIAL deref(daxa_push_constant.material_info_buffer)

#if defined(DRAW_VERT)

layout(location = 0) out f32vec2 out_uv;

void main() {
    out_uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(out_uv * 2.0f - 1.0f, 0.0f, 1.0f);
}


#elif defined(DRAW_FRAG)

layout(location = 0) in f32vec2 in_uv;
layout(location = 0) out f32vec4 out_color;

void main() {
    #if defined(SETTINGS_VISUALIZE_ATTACHMENT_NONE)
    f32vec3 color = sample_texture(daxa_push_constant.albedo, in_uv).rgb;
    f32vec3 position = sample_texture(daxa_push_constant.position, in_uv).rgb;
    f32vec3 normal = normalize(sample_texture(daxa_push_constant.normal, in_uv).rgb);
    f32vec3 camera_position = CAMERA.position;

    for(uint i = 0; i < deref(daxa_push_constant.lights_buffer).num_directional_lights; i++) {
        color += calculate_directional_light(deref(daxa_push_constant.lights_buffer).directional_lights[i], color, normal, position, camera_position);
    }

    for(uint i = 0; i < deref(daxa_push_constant.lights_buffer).num_point_lights; i++) {
        color += calculate_point_light(deref(daxa_push_constant.lights_buffer).point_lights[i], color, normal, position, camera_position);
    }

    for(uint i = 0; i < deref(daxa_push_constant.lights_buffer).num_spot_lights; i++) {
        color += calculate_spot_light(deref(daxa_push_constant.lights_buffer).spot_lights[i], color, normal, position, camera_position);
    }

    #if !defined(SETTINGS_AMBIENT_OCCLUSION_NONE)
    color *= sample_texture(daxa_push_constant.ssao, in_uv).rrr;
    #endif
    
    out_color = vec4(color, 1.0);

    #elif defined(SETTINGS_VISUALIZE_ATTACHMENT_ALBEDO)
    out_color = vec4(sample_texture(daxa_push_constant.albedo, in_uv).rgb, 1.0);
    #elif defined(SETTINGS_VISUALIZE_ATTACHMENT_NORMAL)
    out_color = vec4(sample_texture(daxa_push_constant.normal, in_uv).rgb * 0.5 + 0.5, 1.0);
    #elif defined(SETTINGS_VISUALIZE_ATTACHMENT_POSITION)
    out_color = vec4(sample_texture(daxa_push_constant.position, in_uv).rgb, 1.0);
    #elif defined(SETTINGS_VISUALIZE_ATTACHMENT_DEPTH)
    out_color = vec4(sample_texture(daxa_push_constant.normal, in_uv).www, 1.0);
    #elif defined(SETTINGS_VISUALIZE_ATTACHMENT_AO)
    out_color = vec4(sample_texture(daxa_push_constant.ssao, in_uv).rrr, 1.0);
    #endif

}

#endif