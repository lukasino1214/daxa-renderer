#pragma once
#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <daxa/daxa.inl>

#include "utils/lighting.glsl"
#include "utils/core.glsl"

struct DrawVertex {
    f32vec3 position;
    f32vec3 normal;
    f32vec2 uv;
    f32vec4 tangent;
};

DAXA_ENABLE_BUFFER_PTR(DrawVertex)

struct PositionBuffer {
    f32vec3 position;
};

DAXA_ENABLE_BUFFER_PTR(PositionBuffer)

struct NormalBuffer {
    f32vec3 normal;
};

DAXA_ENABLE_BUFFER_PTR(NormalBuffer)

struct UVBuffer {
    f32vec2 uv;
};

DAXA_ENABLE_BUFFER_PTR(UVBuffer)

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
    //daxa_RWBufferPtr(DrawVertex) face_buffer;
    daxa_RWBufferPtr(PositionBuffer) position_buffer;
    u32 position_offset;
    daxa_RWBufferPtr(NormalBuffer) normal_buffer;
    u32 normal_offset;
    daxa_RWBufferPtr(UVBuffer) uv_buffer;
    u32 uv_offset;
    daxa_RWBufferPtr(MaterialInfo) material_info_buffer;
    daxa_RWBufferPtr(LightsInfo) lights_buffer;
    daxa_RWBufferPtr(CameraInfo) camera_buffer;
    daxa_RWBufferPtr(ObjectInfo) object_buffer;
};

struct CompositionPush {
    TextureId albedo;
    TextureId normal;
    TextureId emissive;
    TextureId depth;
    TextureId ssao;
    daxa_RWBufferPtr(LightsInfo) lights_buffer;
    daxa_RWBufferPtr(CameraInfo) camera_buffer;
};

struct SSAOGenerationPush {
    TextureId normal;
    TextureId depth;
    daxa_RWBufferPtr(CameraInfo) camera_buffer;
};

struct SSAOBlurPush {
    TextureId ssao;
};

struct BloomPush {
    TextureId src_texture;
    f32vec2 src_resolution;
    f32 filter_radius;
};

struct ShadowPush {
    f32mat4x4 light_matrix;
    daxa_RWBufferPtr(PositionBuffer) position_buffer;
    u32 position_offset;
    daxa_RWBufferPtr(ObjectInfo) object_buffer;
    f32vec3 light_position;
    f32 far_plane;
};

struct GaussPush {
    TextureId src_texture;
    f32vec2 blur_scale;
};
