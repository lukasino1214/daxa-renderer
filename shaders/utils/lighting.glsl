#pragma once
#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#include <daxa/daxa.inl>

#include "core.glsl"
#include "shadow.glsl"

struct DirectionalLight {
    f32vec3 direction;
    f32vec3 color;
    f32 intensity;
    TextureId shadow_image;
    f32mat4x4 light_matrix;
    int shadow_type;
};

struct PointLight {
    f32vec3 position;
    f32vec3 color;
    f32 intensity;
    TextureId shadow_image;
    f32mat4x4 light_matrix;
    int shadow_type;
};

struct SpotLight {
    f32vec3 position;
    f32vec3 direction;
    f32vec3 color;
    f32 intensity;
    f32 cut_off;
    f32 outer_cut_off;
    TextureId shadow_image;
    f32mat4x4 light_matrix;
    int shadow_type;
};


#if !defined(__cplusplus)
f32vec3 calculate_directional_light(DirectionalLight light, f32vec3 frag_color, f32vec3 normal, f32vec4 position, f32vec3 camera_position) {
    f32vec4 shadow_coord = light.light_matrix * position;
    f32 shadow = 1.0;
    f32vec3 frag_position = position.xyz;
    f32vec3 light_dir = normalize(-light.direction);
    f32 bias = max(0.05 * (1.0 - dot(normal, light_dir)), 0.005); 

    if(light.shadow_type == 1) {
        shadow = shadow_pcf(light.shadow_image, shadow_coord / shadow_coord.w, bias);
    } else if(light.shadow_type == 2) {
        shadow = variance_shadow(light.shadow_image, shadow_coord / shadow_coord.w);
    }
    
    
    f32vec3 view_dir = normalize(camera_position - frag_position);
    f32vec3 halfway_dir = normalize(light_dir + view_dir);

    f32 diffuse = max(dot(normal, light_dir), 0);
    f32 normal_half = acos(dot(halfway_dir, normal));
    f32 exponent = normal_half / 1.0;
    exponent = -(exponent * exponent);
    return frag_color * light.color * ((diffuse + exp(exponent)) * shadow) * light.intensity;
}

f32vec3 calculate_point_light(PointLight light, f32vec3 frag_color, f32vec3 normal, f32vec4 position, f32vec3 camera_position) {
    f32vec4 shadow_coord = light.light_matrix * position;
    f32 shadow = 1.0;
    f32vec3 frag_position = position.xyz;
    f32vec3 light_dir = normalize(light.position - frag_position);
    f32 bias = max(0.05 * (1.0 - dot(normal, light_dir)), 0.005); 

    if(light.shadow_type == 1) {
        shadow = shadow_pcf(light.shadow_image, shadow_coord / shadow_coord.w, bias);
    } else if(light.shadow_type == 2) {
        shadow = variance_shadow(light.shadow_image, shadow_coord / shadow_coord.w);
    }
    
    f32 distance = length(light.position.xyz - frag_position);
    f32 attenuation = 1.0f / (distance * distance);

    f32vec3 view_dir = normalize(camera_position - frag_position);
    f32vec3 halfway_dir = normalize(light_dir + view_dir);

    f32 diffuse = max(dot(normal, light_dir), 0);
    f32 normal_half = acos(dot(halfway_dir, normal));
    f32 exponent = normal_half * 1.0;
    exponent = -(exponent * exponent);
    return frag_color * light.color * ((diffuse + exp(exponent)) * shadow) * attenuation * light.intensity;
}

f32vec3 calculate_spot_light(SpotLight light, f32vec3 frag_color, f32vec3 normal, f32vec4 position, f32vec3 camera_position) {
    f32vec4 shadow_coord = light.light_matrix * position;
    f32 shadow = 1.0;
    f32vec3 frag_position = position.xyz;
    f32vec3 light_dir = normalize(light.position - frag_position);
    f32 bias = max(0.05 * (1.0 - dot(normal, light_dir)), 0.005); 

    if(light.shadow_type == 1) {
        shadow = shadow_pcf(light.shadow_image, shadow_coord / shadow_coord.w, bias);
    } else if(light.shadow_type == 2) {
        shadow = variance_shadow(light.shadow_image, shadow_coord / shadow_coord.w);
    }
    

    f32 theta = dot(light_dir, normalize(-light.direction)); 
    f32 epsilon = (light.cut_off - light.outer_cut_off);
    f32 intensity = clamp((theta - light.outer_cut_off) / epsilon, 0, 1.0);

    f32 distance = length(light.position - frag_position);
    f32 attenuation = 1.0 / (distance * distance); 

    f32vec3 view_dir = normalize(camera_position - frag_position);
    f32vec3 halfway_dir = normalize(light_dir + view_dir);

    f32 diffuse = max(dot(normal, light_dir), 0);
    f32 normal_half = acos(dot(halfway_dir, normal));
    f32 exponent = normal_half / 1.0;
    exponent = -(exponent * exponent);
    return frag_color * light.color * ((diffuse + exp(exponent)) * shadow) * attenuation * light.intensity * intensity;
}
#endif
