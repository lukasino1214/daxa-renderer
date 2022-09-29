#include <shared.inl>

#define get_texture(texture_id, uv) texture(sampler2D(daxa_GetImage(texture2D, texture_id.image_view_id), daxa_GetSampler(sampler, texture_id.sampler_id)), uv)
#define read_buffer(type, ptr) daxa_buffer_address_to_ref(type, WrappedBufferRef, ptr).value