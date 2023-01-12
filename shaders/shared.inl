#pragma once

//#include "lighting.glsl"

#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#include <daxa/daxa.inl>

#define APPNAME "Daxa Renderer"
#define APPNAME_PREFIX(x) ("[" APPNAME "] " x)

#include "common/lighting.glsl"

struct DrawVertex {
    f32vec3 position;
    f32vec3 normal;
    f32vec2 uv;
    f32vec4 tangent;
};

DAXA_ENABLE_BUFFER_PTR(DrawVertex)

#define MAX_LIGHTS 16

struct LightsInfo {
    i32 num_directional_lights;
    DirectionalLight directional_lights[MAX_LIGHTS];
    i32 num_point_lights;
    PointLight point_lights[MAX_LIGHTS];
    i32 num_spot_lights;
    SpotLight spot_lights[MAX_LIGHTS];
};

DAXA_ENABLE_BUFFER_PTR(LightsInfo)

struct TextureId {
    ImageViewId image_view_id;
    SamplerId sampler_id;
};

struct MaterialInfo {
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
};

DAXA_ENABLE_BUFFER_PTR(MaterialInfo)

struct ObjectInfo {
    f32mat4x4 model_matrix;
    f32mat4x4 normal_matrix;
};

DAXA_ENABLE_BUFFER_PTR(ObjectInfo)

struct CameraInfo {
    f32mat4x4 projection_matrix;
    f32mat4x4 inverse_projection_matrix;
    f32mat4x4 view_matrix;
    f32mat4x4 inverse_view_matrix;
    f32vec3 position;
    f32 near_plane;
    f32 far_plane;
};

DAXA_ENABLE_BUFFER_PTR(CameraInfo)

struct DrawPush {
    daxa_RWBufferPtr(CameraInfo) camera_buffer;
    daxa_RWBufferPtr(ObjectInfo) object_buffer;
    daxa_RWBufferPtr(LightsInfo) lights_buffer;
    daxa_RWBufferPtr(DrawVertex) face_buffer;
    daxa_RWBufferPtr(MaterialInfo) material_info_buffer;
};

struct SkyboxDrawPush {
    f32mat4x4 mvp;
    daxa_RWBufferPtr(DrawVertex) face_buffer;
    TextureId env_map;
};

struct CompositionPush {
    TextureId albedo;
    TextureId normal;
    TextureId position;
    //TextureId ssao;
    daxa_RWBufferPtr(CameraInfo) camera_buffer;
    daxa_RWBufferPtr(LightsInfo) lights_buffer;
};

#define SSAO_KERNEL_SIZE 64
#define SSAO_RADIUS 0.3f
#define SSAO_NOISE_DIM 4


struct SSAOKernel {
    f32vec4 samples[SSAO_KERNEL_SIZE];
};


DAXA_ENABLE_BUFFER_PTR(SSAOKernel)

struct SSAOGenerationPush {
    TextureId normal;
    TextureId position;
    TextureId ssao_noise;
    daxa_RWBufferPtr(CameraInfo) camera_buffer;
    daxa_RWBufferPtr(SSAOKernel) ssao_kernel_buffer;
};

struct SSAOBlurPush {
    TextureId ssao;
};