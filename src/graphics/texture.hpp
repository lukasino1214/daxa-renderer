#pragma once

#include <daxa/command_list.hpp>
#include <daxa/daxa.hpp>
using namespace daxa::types;
#include "../../shaders/shared.inl"

namespace dare {
    struct Texture {
        enum class Type : u8 {
            UNORM = 0,
            SRGB = 1
        };

        Texture();
        Texture(daxa::Device device, u32 size_x, u32 size_y, unsigned char* data, Type type);
        Texture(daxa::Device device, const std::string& path);
        ~Texture();

        static auto load_texture(daxa::Device device, u32 size_x, u32 size_y, unsigned char* data, Type type) -> std::pair<std::unique_ptr<Texture>, daxa::CommandList>;

        auto get_texture_id() -> TextureId;

        daxa::Device device;
        daxa::ImageId image_id;
        daxa::SamplerId sampler_id;
    };
}