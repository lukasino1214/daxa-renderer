#include <shared.inl>
#include <core.glsl>

struct PushData {
    BufferRef(VertexBuffer) face_buffer;
    TextureId hdr;
    f32mat4x4 mvp;
};

DAXA_USE_PUSH_CONSTANT(PushData)

#if defined(DRAW_VERT)
layout(location = 0) out f32vec3 v_position;

void main() {
    DrawVertex vertex = push_constant.face_buffer.vertices[gl_VertexIndex];
    v_position = vertex.position.xyz;
    gl_Position =  push_constant.mvp * vec4(v_position, 1.0);
}

#elif defined(DRAW_FRAG)
layout(location = 0) in f32vec3 v_position;

layout(location = 0) out f32vec4 out_color;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main() {
    vec2 uv = SampleSphericalMap(normalize(v_position));
    out_color = vec4(sample_texture(push_constant.hdr, uv).rgb, 1.0);
}

#endif