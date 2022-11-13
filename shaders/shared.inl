#pragma once

#define DAXA_SHADER_NO_NAMESPACE
#include <daxa/daxa.inl>

DAXA_DECL_BUFFER_STRUCT(DrawVertex, {
    f32vec3 position;
    f32vec3 normal;
    f32vec2 uv;
});

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
    f32 cutOff;
    f32 outerCutOff;     
};

#define MAX_LIGHTS 16

DAXA_DECL_BUFFER_STRUCT(LightsInfo, {
    f32 num_point_lights;
    PointLight point_lights[MAX_LIGHTS];
});

struct TextureId {
    daxa_ImageViewId image_view_id;
    daxa_SamplerId sampler_id;
};

DAXA_DECL_BUFFER_STRUCT(MaterialInfo, {
    TextureId albedo;
    u32 has_albedo;
    f32vec4 albedo_factor;
    TextureId metallic_roughness;
    u32 has_metallic_roughness;
    f32 metallic;
    f32 roughness;
    TextureId normal_map;
    u32 has_normal_map;
    TextureId occlusion_map;
    u32 has_occlusion_map;
    TextureId emissive_map;
    u32 has_emissive_map;
    f32vec3 emissive_factor;
});

DAXA_DECL_BUFFER_STRUCT(ObjectInfo, {
    f32mat4x4 model_matrix;
    f32mat4x4 normal_matrix;
});

DAXA_DECL_BUFFER_STRUCT(CameraInfo, {
    f32mat4x4 projection_matrix;
    f32mat4x4 view_matrix;
    f32vec3 position;
});

struct DrawPush {
    u64 camera_info_buffer;
    u64 object_info_buffer;
    u64 lights_info_buffer;
    u64 face_buffer;
    u64 material_info;
    TextureId irradiance_map;
    TextureId brdfLUT;
    TextureId prefilter_map;
};

struct SkyboxDrawPush {
    f32mat4x4 mvp;
    u64 face_buffer;
    TextureId env_map;
};