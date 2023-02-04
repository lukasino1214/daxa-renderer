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
layout(location = 0) out f32vec4 out_position;

void main() {
    out_position = OBJECT.model_matrix * f32vec4(VERTEX_POSITION, 1.0);
    gl_Position = daxa_push_constant.light_matrix * out_position;
}
#elif defined(DRAW_FRAG)
layout(location = 0) out f32vec2 out_color;

layout(location = 0) in f32vec4 in_position;
void main() {
    f32 dist = length(in_position.xyz - daxa_push_constant.light_position);
    dist /= daxa_push_constant.far_plane;
    
    gl_FragDepth = dist;
    f32 depth = dist;

    f32 dx = dFdx(depth);
    f32 dy = dFdy(depth);
    f32 moment2 = depth * depth + 0.25 * (dx * dx + dy * dy);

    out_color = f32vec2(depth, moment2);
}
#endif