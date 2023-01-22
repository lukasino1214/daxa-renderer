#define SETTINGS_SHADING_MODEL_GAUSSIAN
#include <shared.inl>

DAXA_USE_PUSH_CONSTANT(CompositionPush)

#include "utils/core.glsl"
#include "utils/lighting.glsl"
#include "utils/position_from_depth.glsl"

#define sample_shadow(texture_id, uv, c) textureShadow(texture_id.image_view_id, texture_id.sampler_id, uv, c)

#if defined(DRAW_VERT)
layout(location = 0) out f32vec2 out_uv;

void main() {
    out_uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(out_uv * 2.0f - 1.0f, 0.0f, 1.0f);
}

#elif defined(DRAW_FRAG)

layout(location = 0) out f32vec4 out_color;

layout(location = 0) in f32vec2 in_uv;

vec3 lightPos = vec3(-4.0f, 55.0f, -4.0f);

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

float ShadowCalculation(vec4 shadowCoord, vec2 off) {
    float shadow = 1.0;
	if (shadowCoord.w > -1.0 && shadowCoord.z < 1.0) {
        vec2 kys = shadowCoord.xy * 0.5 + 0.5;
		//float dist = sample_shadow(daxa_push_constant.shadow, kys.xy, shadowCoord.z).r;
        float dist = sample_texture(daxa_push_constant.shadow, shadowCoord.st + off).r;
		if (shadowCoord.w > 0.0 && dist < shadowCoord.z) {
			shadow = 0.1;
		}
	}
	return shadow;

    /*vec2 kys = shadowCoord.xy * 0.5 + 0.5;
	return sample_shadow(daxa_push_constant.shadow, kys.xy, shadowCoord.z).r;*/
}

float ShadowCalculationPCF(vec4 sc) {
    ivec2 texDim = texture_size(daxa_push_constant.shadow, 0);
	float scale = 0.25;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 2;
	
	for (int x = -range; x <= range; x++) {
		for (int y = -range; y <= range; y++) {
			shadowFactor += ShadowCalculation(sc, vec2(dx*x, dy*y));
			count++;
		}
	
	}
	return shadowFactor / count;
}

void main() {
    f32vec4 color = sample_texture(daxa_push_constant.albedo, in_uv);
    f32vec4 normal = sample_texture(daxa_push_constant.normal, in_uv);

    vec3 ambient = vec3(0.75, 0.75, 0.75);
    
    ambient *= sample_texture(daxa_push_constant.ssao, in_uv).r;

    ambient *= color.rgb;

    f32vec4 position = f32vec4(get_world_position_from_depth(in_uv, sample_texture(daxa_push_constant.depth, in_uv).r), 1.0);
    
    f32vec3 camera_position = CAMERA.inverse_view_matrix[3].xyz;

    /*f32vec4 shadow = vec4(biasMat * daxa_push_constant.light_matrix * position);
    ambient *= ShadowCalculationPCF(shadow / shadow.w);*/

    for(uint i = 0; i < LIGHTS.num_directional_lights; i++) {
        ambient += calculate_directional_light(LIGHTS.directional_lights[i], ambient.rgb, normal.xyz, position.xyz, camera_position);
    }

    for(uint i = 0; i < LIGHTS.num_point_lights; i++) {
        ambient += calculate_point_light(LIGHTS.point_lights[i], ambient.rgb, normal.xyz, position.xyz, camera_position);
    }

    for(uint i = 0; i < LIGHTS.num_spot_lights; i++) {
        ambient += calculate_spot_light(LIGHTS.spot_lights[i], ambient.rgb, normal.xyz, position.xyz, camera_position);
    }

    f32vec3 bloom = sample_texture(daxa_push_constant.emissive, in_uv).rgb;

    ambient += bloom * 2;

    f32vec4 shadow = vec4(biasMat * daxa_push_constant.light_matrix * position);
    ambient *= ShadowCalculation(shadow / shadow.w, vec2(0));

    out_color = f32vec4(ambient, 1.0);
}

#endif