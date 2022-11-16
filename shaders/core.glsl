#include <shared.inl>

#define get_texture(texture_id, uv) texture(sampler2D(daxa_GetImage(texture2D, texture_id.image_view_id), daxa_GetSampler(sampler, texture_id.sampler_id)), uv)
#define get_cube_map(texture_id, uv) texture(samplerCube(daxa_GetImage(textureCube, texture_id.image_view_id), daxa_GetSampler(sampler, texture_id.sampler_id)), uv)
#define get_cube_sampler(texture_id) samplerCube(daxa_GetImage(textureCube, texture_id.image_view_id), daxa_GetSampler(sampler, texture_id.sampler_id))
#define get_cube_map_size(texture_id, mip_level) textureSize(samplerCube(daxa_GetImage(textureCube, texture_id.image_view_id), daxa_GetSampler(sampler, texture_id.sampler_id)), mip_level)
#define get_cube_map_lod(texture_id, uv, mip_level) textureLod(samplerCube(daxa_GetImage(textureCube, texture_id.image_view_id), daxa_GetSampler(sampler, texture_id.sampler_id)), uv, mip_level)
#define read_buffer(type, ptr) daxa_buffer_address_to_ref(type, WrappedBufferRef, ptr).value