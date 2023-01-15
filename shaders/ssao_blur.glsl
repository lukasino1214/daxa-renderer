#include <shared.inl>
#include "utils/core.glsl"

DAXA_USE_PUSH_CONSTANT(SSAOBlurPush)

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
    const int blur_range = 2;
	int n = 0;
	vec2 texel_size = 1.0 / vec2(texture_size(daxa_push_constant.ssao, 0));
	float result = 0.0;
	for (int x = -blur_range; x < blur_range; x++) {
		for (int y = -blur_range; y < blur_range; y++) {
			vec2 offset = vec2(float(x), float(y)) * texel_size;
			result += sample_texture(daxa_push_constant.ssao, in_uv + offset).r;
			n++;
		}
	}
	out_ssao = result / (float(n));
}

#endif