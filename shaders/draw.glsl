#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared.inl>

DAXA_USE_PUSH_CONSTANT(DrawPush)

#define VERTEX deref(daxa_push_constant.face_buffer[gl_VertexIndex])
#define OBJECT deref(daxa_push_constant.object_buffer)
#define CAMERA deref(daxa_push_constant.camera_buffer)
#define MATERIAL deref(daxa_push_constant.material_info_buffer)

#define sample_texture(tex, uv) texture(tex.image_view_id, tex.sampler_id, uv)

#if defined(DRAW_VERT)
layout(location = 0) out f32vec3 out_color;
layout(location = 1) out f32vec2 out_uv;

void main() {
    gl_Position = daxa_push_constant.mvp * vec4(VERTEX.position.xyz, 1.0);
    out_uv = VERTEX.uv;
}

#elif defined(DRAW_FRAG)

layout(location = 0) out f32vec4 color;

layout(location = 0) in f32vec3 in_color;
layout(location = 1) in f32vec2 in_uv;

void main() {
    f32vec4 tex_col = sample_texture(MATERIAL.albedo, in_uv);
    color = f32vec4(tex_col.rgb, 1.0);
}

#endif