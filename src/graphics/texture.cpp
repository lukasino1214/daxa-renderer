#include "texture.hpp"

#include <stb_image.h>

#include <cstring>
#include <cmath>

namespace dare {
    Texture::Texture(daxa::Device device, u32 size_x, u32 size_y, unsigned char* data, TextureType type) : device{device} {
        u32 mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(size_x, size_y)))) + 1;

        this->image_id = device.create_image({
            .dimensions = 2,
            .format = (type == TextureType::UNORM) ? daxa::Format::R8G8B8A8_UNORM : daxa::Format::R8G8B8A8_SRGB,
            .aspect = daxa::ImageAspectFlagBits::COLOR,
            .size = { static_cast<u32>(size_x), static_cast<u32>(size_y), 1 },
            .mip_level_count = mip_levels,
            .array_layer_count = 1,
            .sample_count = 1,
            .usage = daxa::ImageUsageFlagBits::SHADER_READ_ONLY | daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::TRANSFER_SRC,
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY
        });

        this->sampler_id = device.create_sampler({
            .magnification_filter = daxa::Filter::LINEAR,
            .minification_filter = daxa::Filter::LINEAR,
            .mipmap_filter = daxa::Filter::LINEAR,
            .address_mode_u = daxa::SamplerAddressMode::REPEAT,
            .address_mode_v = daxa::SamplerAddressMode::REPEAT,
            .address_mode_w = daxa::SamplerAddressMode::REPEAT,
            .mip_lod_bias = 0.0f,
            .enable_anisotropy = true,
            .max_anisotropy = 16.0f,
            .enable_compare = false,
            .compare_op = daxa::CompareOp::ALWAYS,
            .min_lod = 0.0f,
            .max_lod = static_cast<f32>(mip_levels),
            .enable_unnormalized_coordinates = false,
        });

        daxa::BufferId staging_buffer = device.create_buffer({
            .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            .size = static_cast<u32>(size_x * size_y * sizeof(u8) * 4),
        });

        auto buffer_ptr = device.get_host_address_as<u8>(staging_buffer);
        std::memcpy(buffer_ptr, data, size_x * size_y * sizeof(u8) * 4);

        daxa::CommandList cmd_list = device.create_command_list({ .debug_name = "upload texture cmd list" });

        cmd_list.pipeline_barrier_image_transition({
            .awaited_pipeline_access = daxa::AccessConsts::TRANSFER_READ_WRITE,
            .waiting_pipeline_access = daxa::AccessConsts::READ_WRITE,
            .before_layout = daxa::ImageLayout::UNDEFINED,
            .after_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
            .image_slice = {
                .image_aspect = daxa::ImageAspectFlagBits::COLOR,
                .base_mip_level = 0,
                .level_count = mip_levels,
                .base_array_layer = 0,
                .layer_count = 1,
            },
            .image_id = image_id,
        });

        cmd_list.copy_buffer_to_image({
            .buffer = staging_buffer,
            .buffer_offset = 0,
            .image = image_id,
            .image_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
            .image_slice = {
                .image_aspect = daxa::ImageAspectFlagBits::COLOR,
                .mip_level = 0,
                .base_array_layer = 0,
                .layer_count = 1,
            },
            .image_offset = { 0, 0, 0 },
            .image_extent = { static_cast<u32>(size_x), static_cast<u32>(size_y), 1 }
        });

        cmd_list.pipeline_barrier({
            .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
            .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_READ,
        });

        auto image_info = device.info_image(image_id);

        std::array<i32, 3> mip_size = {
            static_cast<i32>(image_info.size.x),
            static_cast<i32>(image_info.size.y),
            static_cast<i32>(image_info.size.z),
        };

        for(u32 i = 1; i < image_info.mip_level_count; i++) {
            cmd_list.pipeline_barrier_image_transition({
                .awaited_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
                .waiting_pipeline_access = daxa::AccessConsts::BLIT_READ,
                .before_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                .after_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                .image_slice = {
                    .image_aspect = image_info.aspect,
                    .base_mip_level = i - 1,
                    .level_count = 1,
                    .base_array_layer = 0,
                    .layer_count = 1,
                },
                .image_id = image_id,
            });

            std::array<i32, 3> next_mip_size = {
                std::max<i32>(1, mip_size[0] / 2),
                std::max<i32>(1, mip_size[1] / 2),
                std::max<i32>(1, mip_size[2] / 2),
            };

            cmd_list.blit_image_to_image({
                .src_image = image_id,
                .src_image_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                .dst_image = image_id,
                .dst_image_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                .src_slice = {
                    .image_aspect = image_info.aspect,
                    .mip_level = i - 1,
                    .base_array_layer = 0,
                    .layer_count = 1,
                },
                .src_offsets = {{{0, 0, 0}, {mip_size[0], mip_size[1], mip_size[2]}}},
                .dst_slice = {
                    .image_aspect = image_info.aspect,
                    .mip_level = i,
                    .base_array_layer = 0,
                    .layer_count = 1,
                },
                .dst_offsets = {{{0, 0, 0}, {next_mip_size[0], next_mip_size[1], next_mip_size[2]}}},
                .filter = daxa::Filter::LINEAR,
            });

            cmd_list.pipeline_barrier_image_transition({
                .awaited_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
                .waiting_pipeline_access = daxa::AccessConsts::BLIT_READ,
                .before_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                .after_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
                .image_slice = {
                    .image_aspect = image_info.aspect,
                    .base_mip_level = i - 1,
                    .level_count = 1,
                    .base_array_layer = 0,
                    .layer_count = 1,
                },
                .image_id = image_id,
            });
            
            mip_size = next_mip_size;
        }

        cmd_list.pipeline_barrier_image_transition({
            .awaited_pipeline_access = daxa::AccessConsts::TRANSFER_READ_WRITE,
            .waiting_pipeline_access = daxa::AccessConsts::READ_WRITE,
            .before_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
            .after_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
            .image_slice = {
                .image_aspect = image_info.aspect,
                .base_mip_level = image_info.mip_level_count - 1,
                .level_count = 1,
                .base_array_layer = 0,
                .layer_count = 1,
            },
            .image_id = image_id,
        });

        cmd_list.complete();

        device.submit_commands({
            .command_lists = {std::move(cmd_list)},
        });
        device.wait_idle();
        device.destroy_buffer(staging_buffer);
    }

    Texture::Texture(daxa::Device device, const std::string& path) : device{device} {
        i32 size_x = 0;
        i32 size_y = 0;
        i32 num_channels = 0;
        u8* data = stbi_load(path.c_str(), &size_x, &size_y, &num_channels, 4);
        if(data == nullptr) {
            throw std::runtime_error("Textures couldn't be found with path: " + path);
        }

        u32 mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(size_x, size_y)))) + 1;

        this->image_id = device.create_image({
            .dimensions = 2,
            .format = daxa::Format::R8G8B8A8_SRGB,
            .aspect = daxa::ImageAspectFlagBits::COLOR,
            .size = { static_cast<u32>(size_x), static_cast<u32>(size_y), 1 },
            .mip_level_count = mip_levels,
            .array_layer_count = 1,
            .sample_count = 1,
            .usage = daxa::ImageUsageFlagBits::SHADER_READ_ONLY | daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::TRANSFER_SRC,
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY
        });

        this->sampler_id = device.create_sampler({
            .magnification_filter = daxa::Filter::LINEAR,
            .minification_filter = daxa::Filter::LINEAR,
            .mipmap_filter = daxa::Filter::LINEAR,
            .address_mode_u = daxa::SamplerAddressMode::REPEAT,
            .address_mode_v = daxa::SamplerAddressMode::REPEAT,
            .address_mode_w = daxa::SamplerAddressMode::REPEAT,
            .mip_lod_bias = 0.0f,
            .enable_anisotropy = true,
            .max_anisotropy = 16.0f,
            .enable_compare = false,
            .compare_op = daxa::CompareOp::ALWAYS,
            .min_lod = 0.0f,
            .max_lod = static_cast<f32>(mip_levels),
            .enable_unnormalized_coordinates = false,
        });

        daxa::BufferId staging_buffer = device.create_buffer({
            .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            .size = static_cast<u32>(size_x * size_y * sizeof(u8) * 4),
        });

        auto buffer_ptr = device.get_host_address_as<u8>(staging_buffer);
        std::memcpy(buffer_ptr, data, size_x * size_y * sizeof(u8) * 4);

        stbi_image_free(data);

        daxa::CommandList cmd_list = device.create_command_list({ .debug_name = "upload texture cmd list" });

        cmd_list.pipeline_barrier_image_transition({
            .awaited_pipeline_access = daxa::AccessConsts::TRANSFER_READ_WRITE,
            .waiting_pipeline_access = daxa::AccessConsts::READ_WRITE,
            .before_layout = daxa::ImageLayout::UNDEFINED,
            .after_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
            .image_slice = {
                .image_aspect = daxa::ImageAspectFlagBits::COLOR,
                .base_mip_level = 0,
                .level_count = mip_levels,
                .base_array_layer = 0,
                .layer_count = 1,
            },
            .image_id = image_id,
        });

        cmd_list.copy_buffer_to_image({
            .buffer = staging_buffer,
            .buffer_offset = 0,
            .image = image_id,
            .image_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
            .image_slice = {
                .image_aspect = daxa::ImageAspectFlagBits::COLOR,
                .mip_level = 0,
                .base_array_layer = 0,
                .layer_count = 1,
            },
            .image_offset = { 0, 0, 0 },
            .image_extent = { static_cast<u32>(size_x), static_cast<u32>(size_y), 1 }
        });

        cmd_list.pipeline_barrier({
            .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
            .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_READ,
        });

        auto image_info = device.info_image(image_id);

        std::array<i32, 3> mip_size = {
            static_cast<i32>(image_info.size.x),
            static_cast<i32>(image_info.size.y),
            static_cast<i32>(image_info.size.z),
        };

        for(u32 i = 1; i < image_info.mip_level_count; i++) {
            cmd_list.pipeline_barrier_image_transition({
                .awaited_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
                .waiting_pipeline_access = daxa::AccessConsts::BLIT_READ,
                .before_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                .after_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                .image_slice = {
                    .image_aspect = image_info.aspect,
                    .base_mip_level = i - 1,
                    .level_count = 1,
                    .base_array_layer = 0,
                    .layer_count = 1,
                },
                .image_id = image_id,
            });

            std::array<i32, 3> next_mip_size = {
                std::max<i32>(1, mip_size[0] / 2),
                std::max<i32>(1, mip_size[1] / 2),
                std::max<i32>(1, mip_size[2] / 2),
            };

            cmd_list.blit_image_to_image({
                .src_image = image_id,
                .src_image_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                .dst_image = image_id,
                .dst_image_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                .src_slice = {
                    .image_aspect = image_info.aspect,
                    .mip_level = i - 1,
                    .base_array_layer = 0,
                    .layer_count = 1,
                },
                .src_offsets = {{{0, 0, 0}, {mip_size[0], mip_size[1], mip_size[2]}}},
                .dst_slice = {
                    .image_aspect = image_info.aspect,
                    .mip_level = i,
                    .base_array_layer = 0,
                    .layer_count = 1,
                },
                .dst_offsets = {{{0, 0, 0}, {next_mip_size[0], next_mip_size[1], next_mip_size[2]}}},
                .filter = daxa::Filter::LINEAR,
            });

            cmd_list.pipeline_barrier_image_transition({
                .awaited_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
                .waiting_pipeline_access = daxa::AccessConsts::BLIT_READ,
                .before_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                .after_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
                .image_slice = {
                    .image_aspect = image_info.aspect,
                    .base_mip_level = i - 1,
                    .level_count = 1,
                    .base_array_layer = 0,
                    .layer_count = 1,
                },
                .image_id = image_id,
            });
            
            mip_size = next_mip_size;
        }

        cmd_list.pipeline_barrier_image_transition({
            .awaited_pipeline_access = daxa::AccessConsts::TRANSFER_READ_WRITE,
            .waiting_pipeline_access = daxa::AccessConsts::READ_WRITE,
            .before_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
            .after_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
            .image_slice = {
                .image_aspect = image_info.aspect,
                .base_mip_level = image_info.mip_level_count - 1,
                .level_count = 1,
                .base_array_layer = 0,
                .layer_count = 1,
            },
            .image_id = image_id,
        });

        cmd_list.complete();

        device.submit_commands({
            .command_lists = {std::move(cmd_list)},
        });
        device.wait_idle();
        device.destroy_buffer(staging_buffer);
    }

    Texture::~Texture() {
        device.destroy_image(this->image_id);
        device.destroy_sampler(this->sampler_id);
    }

    auto Texture::get_texture_id() -> TextureId {
        return TextureId {
                .image_view_id = { image_id },
                .sampler_id = sampler_id
            };
    }
}