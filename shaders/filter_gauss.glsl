#include <shared.inl>

#include "utils/core.glsl"

DAXA_USE_PUSH_CONSTANT(GaussPush)

#define VERTEX deref(daxa_push_constant.face_buffer[gl_VertexIndex])
#define OBJECT deref(daxa_push_constant.object_buffer)
#define CAMERA deref(daxa_push_constant.camera_buffer)
#define MATERIAL deref(daxa_push_constant.material_info_buffer)
#define LIGHTS deref(daxa_push_constant.lights_buffer)

#if defined(DRAW_VERT)
layout(location = 0) out f32vec2 out_uv;

void main() {
    out_uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(out_uv * 2.0f - 1.0f, 0.0f, 1.0f);
}
#elif defined(DRAW_FRAG)
layout(location = 0) in f32vec2 in_uv;
layout(location = 0) out f32vec2 out_color;
void main() {
    vec2 color = vec2(0.0);

	color += sample_texture(daxa_push_constant.src_texture, in_uv + (vec2(-3.0) * daxa_push_constant.blur_scale.xy)).xy * ( 1.0 / 64.0 );
	color += sample_texture(daxa_push_constant.src_texture, in_uv + (vec2(-2.0) * daxa_push_constant.blur_scale.xy)).xy * ( 6.0 / 64.0 );
	color += sample_texture(daxa_push_constant.src_texture, in_uv + (vec2(-1.0) * daxa_push_constant.blur_scale.xy)).xy * (15.0 / 64.0 );
	color += sample_texture(daxa_push_constant.src_texture, in_uv + (vec2(+0.0) * daxa_push_constant.blur_scale.xy)).xy * (20.0 / 64.0 );
	color += sample_texture(daxa_push_constant.src_texture, in_uv + (vec2(+1.0) * daxa_push_constant.blur_scale.xy)).xy * (15.0 / 64.0 );
	color += sample_texture(daxa_push_constant.src_texture, in_uv + (vec2(+2.0) * daxa_push_constant.blur_scale.xy)).xy * ( 6.0 / 64.0 );
	color += sample_texture(daxa_push_constant.src_texture, in_uv + (vec2(+3.0) * daxa_push_constant.blur_scale.xy)).xy * ( 1.0 / 64.0 );

	out_color = color;
}
#endif