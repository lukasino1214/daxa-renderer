#pragma once

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
    ImageViewId image_view_id;
    SamplerId sampler_id;
};

struct PrimtiveInfo {
    TextureId albedo;
    u32 has_albedo;
    f32vec4 albedo_factor;
    TextureId metallic_roughness;
    u32 has_metallic_roughness;
    f32 metallic;
    f32 roughness;
    TextureId normal;
    u32 has_normal;
    TextureId occlusion;
    u32 has_occlusion;
    TextureId emissive;
    u32 has_emissive;
    f32vec3 emissive_factor;
};

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
    f32mat4x4 vp;
    f32vec3 camera_position;
    //u64 camera_info_buffer;
    u64 object_info_buffer;
    u64 lights_info_buffer;
    u64 face_buffer;
    TextureId albedo;
    TextureId metallic_roughness;
    TextureId normal_map;
};