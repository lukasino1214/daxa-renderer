#include <shared.inl>

DAXA_USE_PUSH_CONSTANT(SSAOGenerationPush)

#include "utils/noise.glsl"
#include "utils/core.glsl"
#include "utils/position_from_depth.glsl"

#define KERNEL_SIZE 26

const vec3 kernelSamples[KERNEL_SIZE] = { // HIGH (26 samples)
        vec3(0.2196607,0.9032637,0.2254677),
        vec3(0.05916681,0.2201506,0.1430302),
        vec3(-0.4152246,0.1320857,0.7036734),
        vec3(-0.3790807,0.1454145,0.100605),
        vec3(0.3149606,-0.1294581,0.7044517),
        vec3(-0.1108412,0.2162839,0.1336278),
        vec3(0.658012,-0.4395972,0.2919373),
        vec3(0.5377914,0.3112189,0.426864),
        vec3(-0.2752537,0.07625949,0.1273409),
        vec3(-0.1915639,-0.4973421,0.3129629),
        vec3(-0.2634767,0.5277923,0.1107446),
        vec3(0.8242752,0.02434147,0.06049098),
        vec3(0.06262707,-0.2128643,0.03671562),
        vec3(-0.1795662,-0.3543862,0.07924347),
        vec3(0.06039629,0.24629,0.4501176),
        vec3(-0.7786345,-0.3814852,0.2391262),
        vec3(0.2792919,0.2487278,0.05185341),
        vec3(0.1841383,0.1696993,0.8936281),
        vec3(-0.3479781,0.4725766,0.719685),
        vec3(-0.1365018,-0.2513416,0.470937),
        vec3(0.1280388,-0.563242,0.3419276),
        vec3(-0.4800232,-0.1899473,0.2398808),
        vec3(0.6389147,0.1191014,0.5271206),
        vec3(0.1932822,-0.3692099,0.6060588),
        vec3(-0.3465451,-0.1654651,0.6746758),
        vec3(0.2448421,-0.1610962,0.1289366)
    };

#define SSAO_RADIUS 0.3

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
  
    vec3 frag_position = get_view_position_from_depth(in_uv, sample_texture(daxa_push_constant.depth, in_uv).r);
	vec3 normal = mat3x3(CAMERA.view_matrix) *  normalize(sample_texture(daxa_push_constant.normal, in_uv).rgb);

	ivec2 tex_dim = texture_size(daxa_push_constant.normal, 0); 
	ivec2 noise_dim = texture_size(daxa_push_constant.normal, 0);
	const vec2 noise_uv = vec2(float(tex_dim.x)/float(noise_dim.x), float(tex_dim.y)/(noise_dim.y)) * in_uv;  

    vec3 random_vec = normalize(vec3(
        noise(in_uv, noise_dim.x * 2),
        noise(pow(in_uv, vec2(1.1)), pow(noise_dim.x * 4.2, 1.5 + in_uv.x / 10.0)),
        0.0
    ));

	vec3 tangent = normalize(random_vec - normal * dot(random_vec, normal));
	vec3 bitangent = cross(tangent, normal);
	mat3 TBN = mat3(tangent, bitangent, normal);

	float occlusion = 0.0f;
	const float bias = 0.025f;
	for(int i = 0; i < KERNEL_SIZE; i++) {		
		vec3 sample_pos = TBN * kernelSamples[i].xyz; 
		sample_pos = frag_position + sample_pos * SSAO_RADIUS; 
		
		// project
		vec4 offset = vec4(sample_pos, 1.0f);
		offset = CAMERA.projection_matrix * offset; 
		offset.xy /= offset.w; 
		offset.xy = offset.xy * 0.5f + 0.5f; 
		
		vec3 sample_depth_v = get_view_position_from_depth(offset.xy, sample_texture(daxa_push_constant.depth, offset.xy).r);
        float sample_depth = sample_depth_v.z;

		float range_check = smoothstep(0.0f, 1.0f, SSAO_RADIUS / abs(frag_position.z - sample_depth));
		occlusion += (sample_depth >= sample_pos.z + bias ? 1.0f : 0.0f) * range_check;           
	}
	occlusion = 1.0 - (occlusion / float(KERNEL_SIZE));
	
	out_ssao = occlusion;
}

#endif