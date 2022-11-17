#include <shared.inl>
#include <core.glsl>

DAXA_USE_PUSH_CONSTANT(SkyboxDrawPush)

#if defined(DRAW_VERT)
layout(location = 0) out f32vec3 v_position;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    DrawVertex vertex = push_constant.face_buffer.vertices[gl_VertexIndex];
    v_position = vertex.position.xyz;
    gl_Position = f32vec4(push_constant.mvp * f32vec4(v_position.xyz, 1)).xyww;
}

#elif defined(DRAW_FRAG)
layout(location = 0) in f32vec3 v_position;

layout(location = 0) out f32vec4 out_color;

void main() {
    out_color = vec4(get_cube_map(push_constant.env_map, v_position).rgb, 1.0);
}

#endif