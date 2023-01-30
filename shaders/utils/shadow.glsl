#pragma once
#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#include <daxa/daxa.inl>

#include "core.glsl"

#define SHADOW 0.05

f32 linstep(f32 low, f32 high, f32 v) {
    return clamp((v-low)/(high-low), 0.0, 1.0);
}

f32 normal_shadow(TextureId shadow_image, f32vec4 shadow_coord, f32vec2 off, f32 bias) {
    f32vec3 proj_coord = shadow_coord.xyz * 0.5 + 0.5;
	return max(sample_shadow(shadow_image, proj_coord.xy + off, shadow_coord.z - bias).r, SHADOW);
}

f32 shadow_pcf(TextureId shadow_image, f32vec4 shadow_coord, f32 bias) {
    i32vec2 tex_dim = texture_size(shadow_image, 0);
	f32 scale = 0.25;
	f32 dx = scale * 1.0 / f32(tex_dim.x);
	f32 dy = scale * 1.0 / f32(tex_dim.y);

	f32 shadow_factor = 0.0;
	i32 count = 0;
	i32 range = 2;
	
	for (i32 x = -range; x <= range; x++) {
		for (i32 y = -range; y <= range; y++) {
			shadow_factor += normal_shadow(shadow_image, shadow_coord, f32vec2(dx*x, dy*y), bias);
			count++;
		}
	
	}
	return (shadow_factor / count);
}

f32 variance_shadow(TextureId shadow_image, f32vec4 shadow_coord) {
    f32vec3 proj_coord = shadow_coord.xyz * 0.5 + 0.5;
	
    f32vec2 moments = sample_texture(shadow_image, proj_coord.xy).xy;
    f32 p = step(shadow_coord.z, moments.x);
    f32 variance = max(moments.y - moments.x * moments.x, 0.00002);
	f32 d = shadow_coord.z - moments.x;
	f32 pMax = linstep(0.1, 1.0, variance / (variance + d*d));

    return max(max(p, pMax), SHADOW);
}