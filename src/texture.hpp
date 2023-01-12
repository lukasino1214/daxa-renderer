#pragma once

#include <daxa/daxa.hpp>
using namespace daxa::types;
#include "../shaders/shared.inl"

struct Texture {
    enum class TextureType : u8 {
        UNORM = 0,
        SRGB = 1
    };

    Texture(daxa::Device device, u32 size_x, u32 size_y, unsigned char* data, TextureType type);
    Texture(daxa::Device device, const std::string& path);
    ~Texture();

    auto get_texture_id() -> TextureId;

    daxa::Device device;
    daxa::ImageId image_id;
    daxa::SamplerId sampler_id;
};