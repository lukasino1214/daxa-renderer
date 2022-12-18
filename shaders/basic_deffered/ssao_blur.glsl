#include <shared.inl>
#include <common/core.glsl>

DAXA_USE_PUSH_CONSTANT(SSAOBlurPush)

#define CAMERA deref(daxa_push_constant.camera_buffer)

#if defined(DRAW_VERT)

layout(location = 0) out f32vec2 out_uv;

void main() {
    out_uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(out_uv * 2.0f - 1.0f, 0.0f, 1.0f);
}

#elif defined(DRAW_FRAG)

layout(location = 0) in f32vec2 in_uv;

layout (location = 0) out f32 out_ssao;

void main() {
    const int blurRange = 2;
	int n = 0;
	vec2 texelSize = 1.0 / vec2(texture_size(daxa_push_constant.ssao, 0));
	float result = 0.0;
	for (int x = -blurRange; x < blurRange; x++) 
	{
		for (int y = -blurRange; y < blurRange; y++) 
		{
			vec2 offset = vec2(float(x), float(y)) * texelSize;
			result += sample_texture(daxa_push_constant.ssao, in_uv + offset).r;
			n++;
		}
	}
	out_ssao = result / (float(n));
}

#endif