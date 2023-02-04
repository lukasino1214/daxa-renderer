#pragma once
#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <daxa/daxa.inl>

#define VERTEX_POSITION deref(daxa_push_constant.position_buffer[daxa_push_constant.position_offset + gl_VertexIndex]).position
#define VERTEX_NORMAL deref(daxa_push_constant.normal_buffer[daxa_push_constant.normal_offset + gl_VertexIndex]).normal
#define VERTEX_UV deref(daxa_push_constant.uv_buffer[daxa_push_constant.uv_offset + gl_VertexIndex]).uv
#define OBJECT deref(daxa_push_constant.object_buffer)
#define CAMERA deref(daxa_push_constant.camera_buffer)
#define MATERIAL deref(daxa_push_constant.material_info_buffer)
#define LIGHTS deref(daxa_push_constant.lights_buffer)

struct TextureId {
    daxa_Image2Df32 image_view_id;
    daxa_SamplerId sampler_id;
};

struct TextureCubeId {
    daxa_ImageCubef32 image_view_id;
    daxa_SamplerId sampler_id;
};

#define sample_texture(tex, uv) texture(tex.image_view_id, tex.sampler_id, uv)
#define texture_size(tex, mip) textureSize(tex.image_view_id, mip)
#define sample_shadow(texture_id, uv, c) textureShadow(texture_id.image_view_id, texture_id.sampler_id, uv, c)
#define sample_cube_texture(texture_id, uv) texture(texture_id.image_view_id, texture_id.sampler_id, uv)