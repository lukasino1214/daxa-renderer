#include <shared.inl>


#include "utils/lighting.glsl"
#include "utils/core.glsl"

DAXA_USE_PUSH_CONSTANT(ShadowPush)

#define VERTEX deref(daxa_push_constant.face_buffer[gl_VertexIndex])
#define OBJECT deref(daxa_push_constant.object_buffer)
#define CAMERA deref(daxa_push_constant.camera_buffer)
#define MATERIAL deref(daxa_push_constant.material_info_buffer)
#define LIGHTS deref(daxa_push_constant.lights_buffer)

#if defined(DRAW_VERT)
void main() {
    gl_Position = daxa_push_constant.light_matrix * OBJECT.model_matrix * f32vec4(VERTEX_POSITION, 1.0);
}
#elif defined(DRAW_FRAG)
layout(location = 0) out f32vec2 out_color;
void main() {
    f32 depth = gl_FragCoord.z;

    f32 dx = dFdx(depth);
    f32 dy = dFdy(depth);
    f32 moment2 = depth * depth + 0.25 * (dx * dx + dy * dy);

    out_color = f32vec2(depth, moment2);
}
#endif