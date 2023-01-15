#include <shared.inl>
#include "utils/core.glsl"

DAXA_USE_PUSH_CONSTANT(BloomPush)

#if defined(DRAW_VERT)

layout(location = 0) out f32vec2 out_uv;

void main() {
    out_uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(out_uv * 2.0f - 1.0f, 0.0f, 1.0f);
}

#elif defined(DRAW_FRAG)

layout(location = 0) in f32vec2 in_uv;

layout (location = 0) out f32vec4 out_emissive;

vec3 PowVec3(vec3 v, float p)
{
    return vec3(pow(v.x, p), pow(v.y, p), pow(v.z, p));
}

const float invGamma = 1.0 / 2.2;
vec3 ToSRGB(vec3 v)   { return PowVec3(v, invGamma); }

float sRGBToLuma(vec3 col)
{
    //return dot(col, vec3(0.2126f, 0.7152f, 0.0722f));
	return dot(col, vec3(0.299f, 0.587f, 0.114f));
}

float KarisAverage(vec3 col)
{
	// Formula is 1 / (1 + luma)
	float luma = sRGBToLuma(ToSRGB(col)) * 0.25f;
	return 1.0f / (1.0f + luma);
}

void main() {
    vec2 srcTexelSize = 1.0 / daxa_push_constant.src_resolution;
	float x = srcTexelSize.x;
	float y = srcTexelSize.y;

	// Take 13 samples around current texel:
	// a - b - c
	// - j - k -
	// d - e - f
	// - l - m -
	// g - h - i
	// === ('e' is the current texel) ===
	vec3 a = sample_texture(daxa_push_constant.src_texture, vec2(in_uv.x - 2*x, in_uv.y + 2*y)).rgb;
	vec3 b = sample_texture(daxa_push_constant.src_texture, vec2(in_uv.x,       in_uv.y + 2*y)).rgb;
	vec3 c = sample_texture(daxa_push_constant.src_texture, vec2(in_uv.x + 2*x, in_uv.y + 2*y)).rgb;

	vec3 d = sample_texture(daxa_push_constant.src_texture, vec2(in_uv.x - 2*x, in_uv.y)).rgb;
	vec3 e = sample_texture(daxa_push_constant.src_texture, vec2(in_uv.x,       in_uv.y)).rgb;
	vec3 f = sample_texture(daxa_push_constant.src_texture, vec2(in_uv.x + 2*x, in_uv.y)).rgb;

	vec3 g = sample_texture(daxa_push_constant.src_texture, vec2(in_uv.x - 2*x, in_uv.y - 2*y)).rgb;
	vec3 h = sample_texture(daxa_push_constant.src_texture, vec2(in_uv.x,       in_uv.y - 2*y)).rgb;
	vec3 i = sample_texture(daxa_push_constant.src_texture, vec2(in_uv.x + 2*x, in_uv.y - 2*y)).rgb;

	vec3 j = sample_texture(daxa_push_constant.src_texture, vec2(in_uv.x - x, in_uv.y + y)).rgb;
	vec3 k = sample_texture(daxa_push_constant.src_texture, vec2(in_uv.x + x, in_uv.y + y)).rgb;
	vec3 l = sample_texture(daxa_push_constant.src_texture, vec2(in_uv.x - x, in_uv.y - y)).rgb;
	vec3 m = sample_texture(daxa_push_constant.src_texture, vec2(in_uv.x + x, in_uv.y - y)).rgb;


    out_emissive.rgb = e*0.125;                // ok
    out_emissive.rgb += (a+c+g+i)*0.03125;     // ok
    out_emissive.rgb += (b+d+f+h)*0.0625;      // ok
    out_emissive.rgb += (j+k+l+m)*0.125;       // ok

}

#endif