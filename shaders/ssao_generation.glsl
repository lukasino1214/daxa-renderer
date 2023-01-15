#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared.inl>

DAXA_USE_PUSH_CONSTANT(SSAOGenerationPush)

const vec3 kernelSamples[26] = { // HIGH (26 samples)
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

float rand(vec2 c) {
	return fract(sin(dot(c.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

float noise(vec2 p, float freq) {
	float unit = 2560 / freq;
	vec2 ij = floor(p / unit);
	vec2 xy = mod(p, unit) / unit;
	//xy = 3.*xy*xy-2.*xy*xy*xy;
	xy = .5 * (1. - cos(3.14159265359 * xy));
	float a = rand((ij + vec2(0., 0.)));
	float b = rand((ij + vec2(1., 0.)));
	float c = rand((ij + vec2(0., 1.)));
	float d = rand((ij + vec2(1., 1.)));
	float x1 = mix(a, b, xy.x);
	float x2 = mix(c, d, xy.x);
	return mix(x1, x2, xy.y);
}

float pNoise(vec2 p, int res) {
	float persistance = .5;
	float n = 0.;
	float normK = 0.;
	float f = 4.;
	float amp = 1.;
	int iCount = 0;
	for (int i = 0; i < 50; i++) {
		n += amp * noise(p, f);
		f *= 2.;
		normK += amp;
		amp *= persistance;
		if (iCount == res) break;
		iCount++;
	}
	float nf = n / normK;
	return nf * nf * nf * nf;
}

#define SSAO_RADIUS 0.3

#define VERTEX deref(daxa_push_constant.face_buffer[gl_VertexIndex])
#define OBJECT deref(daxa_push_constant.object_buffer)
#define CAMERA deref(daxa_push_constant.camera_buffer)
#define MATERIAL deref(daxa_push_constant.material_info_buffer)
#define LIGHTS deref(daxa_push_constant.lights_buffer)

#define sample_texture(tex, uv) texture(tex.image_view_id, tex.sampler_id, uv)
#define texture_size(tex, mip) textureSize(tex.image_view_id, tex.sampler_id, mip)

#if defined(DRAW_VERT)

layout(location = 0) out f32vec2 out_uv;

void main() {
    out_uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(out_uv * 2.0f - 1.0f, 0.0f, 1.0f);
}

#elif defined(DRAW_FRAG)

layout(location = 0) in f32vec2 in_uv;

layout (location = 0) out f32 out_ssao;

vec3 get_view_position_from_depth(vec2 uv, float depth) {
    vec4 clipSpacePosition = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewSpacePosition = CAMERA.inverse_projection_matrix * clipSpacePosition;

    viewSpacePosition /= viewSpacePosition.w;

    return viewSpacePosition.xyz;
}

void main() {
  
    vec3 fragPos = get_view_position_from_depth(in_uv, sample_texture(daxa_push_constant.depth, in_uv).r);
	vec3 normal = mat3x3(CAMERA.view_matrix) *  normalize(sample_texture(daxa_push_constant.normal, in_uv).rgb);
    

	// Get a random vector using a noise lookup
	ivec2 texDim = texture_size(daxa_push_constant.normal, 0); 
	ivec2 noiseDim = texture_size(daxa_push_constant.normal, 0);
	const vec2 noiseUV = vec2(float(texDim.x)/float(noiseDim.x), float(texDim.y)/(noiseDim.y)) * in_uv;  
	
    
    //vec3 randomVec = sample_texture(daxa_push_constant.ssao_noise, noiseUV).xyz * 2.0 - 1.0;
    vec3 randomVec = normalize(vec3(
        noise(in_uv, noiseDim.x * 2),
        noise(pow(in_uv, vec2(1.1)), pow(noiseDim.x * 4.2, 1.5 + in_uv.x / 10.0)),
        0.0
    ));
	
	// Create TBN matrix
	vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent = cross(tangent, normal);
	mat3 TBN = mat3(tangent, bitangent, normal);

	// Calculate occlusion value
	float occlusion = 0.0f;
	// remove banding
	const float bias = 0.025f;
	for(int i = 0; i < 26; i++)
	{		
		vec3 samplePos = TBN * kernelSamples[i].xyz; 
		samplePos = fragPos + samplePos * SSAO_RADIUS; 
		
		// project
		vec4 offset = vec4(samplePos, 1.0f);
		offset = CAMERA.projection_matrix * offset; 
		offset.xy /= offset.w; 
		offset.xy = offset.xy * 0.5f + 0.5f; 
		
		vec3 sampleDepthVector = get_view_position_from_depth(offset.xy, sample_texture(daxa_push_constant.depth, offset.xy).r);
        float sampleDepth = sampleDepthVector.z;

		float rangeCheck = smoothstep(0.0f, 1.0f, SSAO_RADIUS / abs(fragPos.z - sampleDepth));
		occlusion += (sampleDepth >= samplePos.z + bias ? 1.0f : 0.0f) * rangeCheck;           
	}
	occlusion = 1.0 - (occlusion / float(26));
	
	out_ssao = occlusion;
}

#endif