#pragma once
#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#include <daxa/daxa.inl>

struct DirectionalLight {
    f32vec3 direction;
    f32vec3 color;
    f32 intensity;
};

struct PointLight {
    f32vec3 position;
    f32vec3 color;
    f32 intensity;
};

struct SpotLight {
    f32vec3 position;
    f32vec3 direction;
    f32vec3 color;
    f32 intensity;
    f32 cut_off;
    f32 outer_cut_off;     
};

#if !defined(__cplusplus)
f32vec3 calculate_directional_light(DirectionalLight light, f32vec3 frag_color, f32vec3 normal, f32vec3 frag_position, f32vec3 camera_position) {
    f32vec3 light_dir = normalize(-light.direction);
    
#if defined(SETTINGS_SHADING_MODEL_LAMBERTIAN)
    f32vec3 diffuse = frag_color * light.color * max(dot(normal, light_dir), 0.0) * light.intensity;
    return diffuse;
#endif
#if defined(SETTINGS_SHADING_MODEL_PHONG)
    f32vec3 view_dir = normalize(camera_position - frag_position);

    f32 diffuse = max(dot(normal, light_dir), 0.0);
    f32 specular = max(dot(view_dir, reflect(-light_dir, normal)), 0.0);
    return frag_color * light.color * (diffuse + specular) * light.intensity;
#endif
#if defined(SETTINGS_SHADING_MODEL_BLINN_PHONG)
    f32vec3 view_dir = normalize(camera_position - frag_position);
    f32vec3 halfway_dir = normalize(light_dir + view_dir);

    f32 diffuse = max(dot(normal, light_dir), 0.0);
    f32 specular = max(dot(view_dir, halfway_dir), 0.0);
    return frag_color * light.color * (diffuse + specular) * light.intensity;
#endif
#if defined(SETTINGS_SHADING_MODEL_GAUSSIAN)
    f32vec3 view_dir = normalize(camera_position - frag_position);
    f32vec3 halfway_dir = normalize(light_dir + view_dir);

    f32 diffuse = max(dot(normal, light_dir), 0.0);
    f32 normal_half = acos(dot(halfway_dir, normal));
    f32 exponent = normal_half / 1.0;
    exponent = -(exponent * exponent);
    return frag_color * light.color * (diffuse + exp(exponent)) * light.intensity;
#endif
    return f32vec3(0.0);
}

f32vec3 calculate_point_light(PointLight light, f32vec3 frag_color, f32vec3 normal, f32vec3 frag_position, f32vec3 camera_position) {
    f32vec3 light_dir = normalize(light.position - frag_position);
    f32 distance = length(light.position.xyz - frag_position);
    f32 attenuation = 1.0f / (distance * distance);

#if defined(SETTINGS_SHADING_MODEL_LAMBERTIAN)
    f32vec3 diffuse = frag_color * light.color * max(dot(normal, light_dir), 0.0) * attenuation * light.intensity;
    return diffuse;
#endif
#if defined(SETTINGS_SHADING_MODEL_PHONG)
    f32vec3 view_dir = normalize(camera_position - frag_position);

    f32 diffuse = max(dot(normal, light_dir), 0.0);
    f32 specular = max(dot(view_dir, reflect(-light_dir, normal)), 0.0);
    return frag_color * light.color * (diffuse + specular) * attenuation * light.intensity;
#endif
#if defined(SETTINGS_SHADING_MODEL_BLINN_PHONG)
    f32vec3 view_dir = normalize(camera_position - frag_position);
    f32vec3 halfway_dir = normalize(light_dir + view_dir);

    f32 diffuse = max(dot(normal, light_dir), 0.0);
    f32 specular = max(dot(view_dir, halfway_dir), 0.0);
    return frag_color * light.color * (diffuse + specular) * attenuation * light.intensity;
#endif
#if defined(SETTINGS_SHADING_MODEL_GAUSSIAN)
    f32vec3 view_dir = normalize(camera_position - frag_position);
    f32vec3 halfway_dir = normalize(light_dir + view_dir);

    f32 diffuse = max(dot(normal, light_dir), 0.0);
    f32 normal_half = acos(dot(halfway_dir, normal));
    f32 exponent = normal_half / 1.0;
    exponent = -(exponent * exponent);
    return frag_color * light.color * (diffuse + exp(exponent)) * attenuation * light.intensity;
#endif
    return f32vec3(0.0);
}

f32vec3 calculate_spot_light(SpotLight light, f32vec3 frag_color, f32vec3 normal, f32vec3 frag_position, f32vec3 camera_position) {
    f32vec3 light_dir = normalize(light.position - frag_position);

    f32 theta = dot(light_dir, normalize(-light.direction)); 
    f32 epsilon = (light.cut_off - light.outer_cut_off);
    f32 intensity = clamp((theta - light.outer_cut_off) / epsilon, 0.0, 1.0);

    f32 distance = length(light.position - frag_position);
    f32 attenuation = 1.0 / (distance * distance); 

#if defined(SETTINGS_SHADING_MODEL_LAMBERTIAN)
    f32vec3 diffuse = frag_color * light.color * max(dot(normal, light_dir), 0.0) * attenuation * light.intensity * intensity;
    return diffuse;

#endif
#if defined(SETTINGS_SHADING_MODEL_PHONG)
    f32vec3 view_dir = normalize(camera_position - frag_position);

    f32 diffuse = max(dot(normal, light_dir), 0.0);
    f32 specular = max(dot(view_dir, reflect(-light_dir, normal)), 0.0);
    return frag_color * light.color * (diffuse + specular) * attenuation * light.intensity * intensity;
#endif
#if defined(SETTINGS_SHADING_MODEL_BLINN_PHONG)
    f32vec3 view_dir = normalize(camera_position - frag_position);
    f32vec3 halfway_dir = normalize(light_dir + view_dir);

    f32 diffuse = max(dot(normal, light_dir), 0.0);
    f32 specular = max(dot(view_dir, halfway_dir), 0.0);
    return frag_color * light.color * (diffuse + specular) * attenuation * light.intensity * intensity;
#endif
#if defined(SETTINGS_SHADING_MODEL_GAUSSIAN)
    f32vec3 view_dir = normalize(camera_position - frag_position);
    f32vec3 halfway_dir = normalize(light_dir + view_dir);

    f32 diffuse = max(dot(normal, light_dir), 0.0);
    f32 normal_half = acos(dot(halfway_dir, normal));
    f32 exponent = normal_half / 1.0;
    exponent = -(exponent * exponent);
    return frag_color * light.color * (diffuse + exp(exponent)) * attenuation * light.intensity * intensity;
#endif
    return f32vec3(0.0);
}
#endif