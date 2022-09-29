#pragma once

#include <daxa/daxa.hpp>
#include <stb_image.h>
#include <cmath>
#include <cstring>
#include <filesystem>

using namespace daxa::types;
#include "../../shaders/shared.inl"


enum class TextureType : u8 {
    UNORM = 0,
    SRGB = 1
};

struct Texture {
    daxa::ImageId image_id;
    daxa::SamplerId sampler_id;

    static Texture load(daxa::Device& device, u32 width, u32 height, unsigned char* data, TextureType type);
    static Texture load(daxa::Device& device, const std::filesystem::path& path);

    inline static void generate_mipmaps(daxa::CommandList& cmd_list, const daxa::ImageInfo& image_info, const daxa::ImageId& image);
    
    TextureId get_texture_id();

    void destroy(daxa::Device& device);
};