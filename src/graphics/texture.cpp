#include "texture.hpp"

Texture Texture::load(daxa::Device& device, u32 width, u32 height, unsigned char* data, TextureType type) {
    u32 mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

    daxa::ImageId image = device.create_image({
        .dimensions = 2,
        .format = (type == TextureType::UNORM) ? daxa::Format::R8G8B8A8_UNORM : daxa::Format::R8G8B8A8_SRGB,
        .aspect = daxa::ImageAspectFlagBits::COLOR,
        .size = { static_cast<u32>(width), static_cast<u32>(height), 1 },
        .mip_level_count = mip_levels,
        .array_layer_count = 1,
        .sample_count = 1,
        .usage = daxa::ImageUsageFlagBits::SHADER_READ_ONLY | daxa::ImageUsageFlagBits::TRANSFER_SRC | daxa::ImageUsageFlagBits::TRANSFER_DST,
        .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY
    });

    daxa::BufferId staging_buffer = device.create_buffer({
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
        .size = static_cast<u32>(width * height * sizeof(u8) * 4),
    });

    auto staging_buffer_ptr = device.map_memory_as<u8>(staging_buffer);
    std::memcpy(staging_buffer_ptr, data, width * height * sizeof(u8) * 4);
    device.unmap_memory(staging_buffer);

    auto cmd_list = device.create_command_list({});
    cmd_list.pipeline_barrier({
        .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
        .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_READ,
    });
    cmd_list.pipeline_barrier_image_transition({
        .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
        .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_READ,
        .before_layout = daxa::ImageLayout::UNDEFINED,
        .after_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
        .image_id = image,
        .image_slice = {
            .base_mip_level = 0,
            .level_count = mip_levels,
            .base_array_layer = 0,
            .layer_count = 1
        }
    });
    cmd_list.copy_buffer_to_image({
        .buffer = staging_buffer,
        .buffer_offset = 0,
        .image = image,
        .image_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
        .image_slice = {
            .image_aspect = daxa::ImageAspectFlagBits::COLOR,
            .mip_level = 0,
            .base_array_layer = 0,
            .layer_count = 1,
        },
        .image_offset = { 0, 0, 0 },
        .image_extent = { static_cast<u32>(width), static_cast<u32>(height), 1 }
    });
    cmd_list.pipeline_barrier({
        .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
        .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_READ,
    });

    auto image_info = device.info_image(image);
    generate_mipmaps(cmd_list, image_info, image);

    cmd_list.complete();
    device.submit_commands({
        .command_lists = {std::move(cmd_list)},
    });
    device.wait_idle();
    device.destroy_buffer(staging_buffer);

    daxa::SamplerId sampler = device.create_sampler({
        .magnification_filter = daxa::Filter::LINEAR,
        .minification_filter = daxa::Filter::LINEAR,
        .mipmap_filter = daxa::Filter::LINEAR,
        .adress_mode_u = daxa::SamplerAddressMode::REPEAT,
        .adress_mode_v = daxa::SamplerAddressMode::REPEAT,
        .adress_mode_w = daxa::SamplerAddressMode::REPEAT,
        .mip_lod_bias = 0.0f,
        .enable_anisotropy = false,
        .max_anisotropy = 0.0f,
        .enable_compare = false,
        .compareOp = daxa::CompareOp::ALWAYS,
        .min_lod = 0.0f,
        .max_lod = static_cast<f32>(mip_levels),
        .enable_unnormalized_coordinates = false,
    });

    return Texture {
        .image_id = image,
        .sampler_id = sampler
    };
}

Texture Texture::load(daxa::Device& device, const std::filesystem::path& path) {
    i32 channels, bytes_per_pixel, width, height;

    auto data = stbi_load(path.c_str(), &width, &height, &bytes_per_pixel, 4);
    u32 mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

    daxa::ImageId image = device.create_image({
        .dimensions = 2,
        .format = daxa::Format::R8G8B8A8_SRGB,
        .aspect = daxa::ImageAspectFlagBits::COLOR,
        .size = { static_cast<u32>(width), static_cast<u32>(height), 1 },
        .mip_level_count = mip_levels,
        .array_layer_count = 1,
        .sample_count = 1,
        .usage = daxa::ImageUsageFlagBits::SHADER_READ_ONLY | daxa::ImageUsageFlagBits::TRANSFER_SRC | daxa::ImageUsageFlagBits::TRANSFER_DST,
        .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY
    });

    daxa::BufferId staging_buffer = device.create_buffer({
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
        .size = static_cast<u32>(width * height * sizeof(u8) * 4),
    });

    auto staging_buffer_ptr = device.map_memory_as<u8>(staging_buffer);
    std::memcpy(staging_buffer_ptr, data, width * height * sizeof(u8) * 4);
    device.unmap_memory(staging_buffer);

    stbi_image_free(data);

    auto cmd_list = device.create_command_list({});
    cmd_list.pipeline_barrier({
        .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
        .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_READ,
    });
    cmd_list.pipeline_barrier_image_transition({
        .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
        .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_READ,
        .before_layout = daxa::ImageLayout::UNDEFINED,
        .after_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
        .image_id = image,
        .image_slice = {
            .base_mip_level = 0,
            .level_count = mip_levels,
            .base_array_layer = 0,
            .layer_count = 1
        }
    });
    cmd_list.copy_buffer_to_image({
        .buffer = staging_buffer,
        .buffer_offset = 0,
        .image = image,
        .image_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
        .image_slice = {
            .image_aspect = daxa::ImageAspectFlagBits::COLOR,
            .mip_level = 0,
            .base_array_layer = 0,
            .layer_count = 1,
        },
        .image_offset = { 0, 0, 0 },
        .image_extent = { static_cast<u32>(width), static_cast<u32>(height), 1 }
    });
    cmd_list.pipeline_barrier({
        .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
        .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_READ,
    });

    auto image_info = device.info_image(image);
    generate_mipmaps(cmd_list, image_info, image);

    cmd_list.complete();
    device.submit_commands({
        .command_lists = {std::move(cmd_list)},
    });
    device.wait_idle();
    device.destroy_buffer(staging_buffer);

    daxa::SamplerId sampler = device.create_sampler({
        .magnification_filter = daxa::Filter::LINEAR,
        .minification_filter = daxa::Filter::LINEAR,
        .mipmap_filter = daxa::Filter::LINEAR,
        .adress_mode_u = daxa::SamplerAddressMode::REPEAT,
        .adress_mode_v = daxa::SamplerAddressMode::REPEAT,
        .adress_mode_w = daxa::SamplerAddressMode::REPEAT,
        .mip_lod_bias = 0.0f,
        .enable_anisotropy = true,
        .max_anisotropy = 16.0f,
        .enable_compare = false,
        .compareOp = daxa::CompareOp::ALWAYS,
        .min_lod = 0.0f,
        .max_lod = static_cast<f32>(mip_levels),
        .enable_unnormalized_coordinates = false,
    });

    return Texture {
        .image_id = image,
        .sampler_id = sampler
    };
}

void Texture::generate_mipmaps(daxa::CommandList& cmd_list, const daxa::ImageInfo& image_info, const daxa::ImageId& image) {
    std::array<i32, 3> mip_size = {
        static_cast<i32>(image_info.size[0]),
        static_cast<i32>(image_info.size[1]),
        static_cast<i32>(image_info.size[2]),
    };
    for (u32 i = 0; i < image_info.mip_level_count - 1; ++i) {
        cmd_list.pipeline_barrier_image_transition({
            .awaited_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
            .waiting_pipeline_access = daxa::AccessConsts::BLIT_READ,
            .before_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
            .after_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
            .image_id = image,
            .image_slice = {
                .image_aspect = image_info.aspect,
                .base_mip_level = i,
                .level_count = 1,
                .base_array_layer = 0,
                .layer_count = 1,
            },
        });
        cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::BLIT_READ,
            .after_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
            .image_id = image,
            .image_slice = {
                .image_aspect = image_info.aspect,
                .base_mip_level = i + 1,
                .level_count = 1,
                .base_array_layer = 0,
                .layer_count = 1,
            },
        });
        std::array<i32, 3> next_mip_size = {
            std::max<i32>(1, mip_size[0] / 2),
            std::max<i32>(1, mip_size[1] / 2),
            std::max<i32>(1, mip_size[2] / 2),
        };
        cmd_list.blit_image_to_image({
            .src_image = image,
            .src_image_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
            .dst_image = image,
            .dst_image_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
            .src_slice = {
                .image_aspect = image_info.aspect,
                .mip_level = i,
                .base_array_layer = 0,
                .layer_count = 1,
            },
            .src_offsets = {{{0, 0, 0}, {mip_size[0], mip_size[1], mip_size[2]}}},
            .dst_slice = {
                .image_aspect = image_info.aspect,
                .mip_level = i + 1,
                .base_array_layer = 0,
                .layer_count = 1,
            },
            .dst_offsets = {{{0, 0, 0}, {next_mip_size[0], next_mip_size[1], next_mip_size[2]}}},
            .filter = daxa::Filter::LINEAR,
        });
        mip_size = next_mip_size;
    }
    for (u32 i = 0; i < image_info.mip_level_count - 1; ++i)
    {
        cmd_list.pipeline_barrier_image_transition({
            .awaited_pipeline_access = daxa::AccessConsts::TRANSFER_READ_WRITE,
            .waiting_pipeline_access = daxa::AccessConsts::READ_WRITE,
            .before_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
            .after_layout = daxa::ImageLayout::SHADER_READ_ONLY_OPTIMAL,
            .image_id = image,
            .image_slice = {
                .image_aspect = image_info.aspect,
                .base_mip_level = i,
                .level_count = 1,
                .base_array_layer = 0,
                .layer_count = 1,
            },
        });
    }
    cmd_list.pipeline_barrier_image_transition({
        .awaited_pipeline_access = daxa::AccessConsts::TRANSFER_READ_WRITE,
        .waiting_pipeline_access = daxa::AccessConsts::READ_WRITE,
        .before_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
        .after_layout = daxa::ImageLayout::SHADER_READ_ONLY_OPTIMAL,
        .image_id = image,
        .image_slice = {
            .image_aspect = image_info.aspect,
            .base_mip_level = image_info.mip_level_count - 1,
            .level_count = 1,
            .base_array_layer = 0,
            .layer_count = 1,
        },
    });
}

TextureId Texture::get_texture_id() {
    return TextureId {
        .image_view_id = { image_id },
        .sampler_id = sampler_id
    };
}

void Texture::destroy(daxa::Device& device) {
    device.destroy_image(image_id);
    device.destroy_sampler(sampler_id);
}