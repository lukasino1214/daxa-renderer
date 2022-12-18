#include <shared.inl>
#include <common/core.glsl>

DAXA_USE_PUSH_CONSTANT(SSAOGenerationPush)

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
    
        vec3 fragPos = (CAMERA.view_matrix * f32vec4(sample_texture(daxa_push_constant.position, in_uv).rgb, 0.0)).xyz;
	    vec3 normal = (normalize(CAMERA.inverse_view_matrix * f32vec4(normalize(sample_texture(daxa_push_constant.normal, in_uv).rgb), 0.0))).xyz;
    
    /*vec3 fragPos = sample_texture(daxa_push_constant.position, in_uv).rgb;
	vec3 normal = normalize(sample_texture(daxa_push_constant.normal, in_uv).rgb * 2.0 - 1.0);*/

	// Get a random vector using a noise lookup
	ivec2 texDim = texture_size(daxa_push_constant.position, 0); 
	ivec2 noiseDim = texture_size(daxa_push_constant.ssao_noise, 0);
	const vec2 noiseUV = vec2(float(texDim.x)/float(noiseDim.x), float(texDim.y)/(noiseDim.y)) * in_uv;  
	vec3 randomVec = sample_texture(daxa_push_constant.ssao_noise, noiseUV).xyz;
	
	// Create TBN matrix
	vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent = cross(tangent, normal);
	mat3 TBN = mat3(tangent, bitangent, normal);

	// Calculate occlusion value
	float occlusion = 0.0f;
	// remove banding
	const float bias = 0.025f;
	for(int i = 0; i < SSAO_KERNEL_SIZE; i++)
	{		
		vec3 samplePos = TBN * deref(daxa_push_constant.ssao_kernel_buffer).samples[i].xyz; 
		samplePos = fragPos + samplePos * SSAO_RADIUS; 
		
		// project
		vec4 offset = vec4(samplePos, 1.0f);
		offset = CAMERA.projection_matrix * offset; 
		offset.xyz /= offset.w; 
		offset.xyz = offset.xyz * 0.5f + 0.5f; 
		
		float sampleDepth = -sample_texture(daxa_push_constant.normal, offset.xy).w; 

		float rangeCheck = smoothstep(0.0f, 1.0f, SSAO_RADIUS / abs(fragPos.z - sampleDepth));
		occlusion += (sampleDepth >= samplePos.z + bias ? 1.0f : 0.0f) * rangeCheck;           
	}
	occlusion = 1.0 - (occlusion / float(SSAO_KERNEL_SIZE));
	
	out_ssao = occlusion;
}

#endif