#include <shared.inl>
#include <core.glsl>

struct PushData {
    BufferRef(VertexBuffer) face_buffer;
    TextureId env_map;
    f32 delta_phi;
    f32 delta_theta;
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

#define PI 3.1415926535897932384626433832795

void main() {
    vec3 N = normalize(v_position);
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up = cross(N, right);

    const float TWO_PI = PI * 2.0;
    const float HALF_PI = PI * 0.5;

    vec3 color = vec3(0.0);
    uint sampleCount = 0u;
    for (float phi = 0.0; phi < TWO_PI; phi += push_constant.delta_phi) {
        for (float theta = 0.0; theta < HALF_PI; theta += push_constant.delta_theta) {
            vec3 tempVec = cos(phi) * right + sin(phi) * up;
            vec3 sampleVector = cos(theta) * N + sin(theta) * tempVec;
            color += get_cube_map(push_constant.env_map, sampleVector).rgb * cos(theta) * sin(theta);
            sampleCount++;
        }
    }
    out_color = vec4(PI * color / float(sampleCount), 1.0);
}

#endif