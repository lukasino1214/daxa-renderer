#include "ibl_renderer.hpp"

#include "../graphics/texture.hpp"

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

#include <daxa/daxa.hpp>

namespace dare {
    IBLRenderer::IBLRenderer(daxa::Device& device, daxa::PipelineCompiler& pipeline_compiler, daxa::Format swapchain_format) : device{device} {
        skybox_pipeline = pipeline_compiler.create_raster_pipeline({
            .vertex_shader_info = {.source = daxa::ShaderFile{"skybox_draw.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_VERT"}}}},
            .fragment_shader_info = {.source = daxa::ShaderFile{"skybox_draw.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_FRAG"}}}},
            .color_attachments = {{.format = swapchain_format, .blend = {.blend_enable = true, .src_color_blend_factor = daxa::BlendFactor::SRC_ALPHA, .dst_color_blend_factor = daxa::BlendFactor::ONE_MINUS_SRC_ALPHA}}},
            .depth_test = {
                .depth_attachment_format = daxa::Format::D24_UNORM_S8_UINT,
                .enable_depth_test = true,
                .enable_depth_write = true,
                .depth_test_compare_op = daxa::CompareOp::LESS_OR_EQUAL
            },
            .raster = {
                .polygon_mode = daxa::PolygonMode::FILL,
            },
            .push_constant_size = sizeof(SkyboxDrawPush)
        }).value();
        cube_model = std::make_unique<Model>(device, "assets/models/cube.gltf");

        daxa::ImageId BRDFLUT_image = device.create_image({
            .dimensions = 2,
            .format = daxa::Format::R16G16_SFLOAT,
            .size = { 512, 512, 1 },
            .mip_level_count = 1,
            .array_layer_count = 1, 
            .sample_count = 1,
            .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY
        });

        daxa::SamplerId BRDFLUT_sampler = device.create_sampler({
            .magnification_filter = daxa::Filter::LINEAR,
            .minification_filter = daxa::Filter::LINEAR,
            .mipmap_filter = daxa::Filter::LINEAR,
            .address_mode_u = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
            .address_mode_v = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
            .address_mode_w = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
            .min_lod = 0.0f,
            .max_lod = 1.0f,
        });

        {
            daxa::RasterPipeline BRDFLUT_pipeline = pipeline_compiler.create_raster_pipeline({
                .vertex_shader_info = {.source = daxa::ShaderFile{"bdrflut.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_VERT"}}}},
                .fragment_shader_info = {.source = daxa::ShaderFile{"bdrflut.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_FRAG"}}}},
                .color_attachments = {{.format = daxa::Format::R16G16_SFLOAT, .blend = {.blend_enable = true, .src_color_blend_factor = daxa::BlendFactor::SRC_ALPHA, .dst_color_blend_factor = daxa::BlendFactor::ONE_MINUS_SRC_ALPHA}}},
                .raster = {
                    .polygon_mode = daxa::PolygonMode::FILL,
                    .face_culling = daxa::FaceCullFlagBits::BACK_BIT,
                },
            }).value();

            daxa::CommandList cmd_list = device.create_command_list({});

            cmd_list.pipeline_barrier_image_transition({
                .waiting_pipeline_access = daxa::AccessConsts::COLOR_ATTACHMENT_OUTPUT_WRITE,
                .before_layout = daxa::ImageLayout::UNDEFINED,
                .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                .image_id = BRDFLUT_image,
            });

            cmd_list.set_pipeline(BRDFLUT_pipeline);

            cmd_list.begin_renderpass({
                .color_attachments = {{
                    .image_view = BRDFLUT_image.default_view(),
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<f32, 4>{ 0.0f, 0.0f, 0.0f, 0.0f },
                }},
                .render_area = {.x = 0, .y = 0, .width = 512, .height = 512},
            });

            cmd_list.draw({
                .vertex_count = 3,
                .instance_count = 1,
                .first_vertex = 0,
                .first_instance = 0
            });

            cmd_list.end_renderpass();
            cmd_list.pipeline_barrier_image_transition({
                .waiting_pipeline_access = daxa::AccessConsts::COLOR_ATTACHMENT_OUTPUT_WRITE,
                .before_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                .after_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
                .image_id = BRDFLUT_image,
            });

            cmd_list.complete();
            device.submit_commands({
                .command_lists = {std::move(cmd_list)}
            });
            device.wait_idle();
        }

        const u32 env_map_mip_levels = static_cast<uint32_t>(std::floor(std::log2(512))) + 1;

        env_map_image = device.create_image({
            .dimensions = 2,
            .format = daxa::Format::R32G32B32A32_SFLOAT,
            .size = { 512, 512, 1 },
            .mip_level_count = env_map_mip_levels,
            .array_layer_count = 6, 
            .sample_count = 1,
            .usage = daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY
        });

        daxa::ImageViewId env_map_image_view = device.create_image_view({
            .type = daxa::ImageViewType::CUBE,
            .format = daxa::Format::R32G32B32A32_SFLOAT,
            .image = env_map_image,
            .slice = {
                .image_aspect = daxa::ImageAspectFlagBits::COLOR,
                .base_mip_level = 0,
                .level_count = env_map_mip_levels,
                .base_array_layer = 0,
                .layer_count = 6

            }
        });

        daxa::SamplerId env_map_sampler = device.create_sampler({
            .magnification_filter = daxa::Filter::LINEAR,
            .minification_filter = daxa::Filter::LINEAR,
            .mipmap_filter = daxa::Filter::LINEAR,
            .address_mode_u = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
            .address_mode_v = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
            .address_mode_w = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
            .min_lod = 0.0f,
            .max_lod = static_cast<f32>(env_map_mip_levels),
        });

        {
            int width, height, channels;

            stbi_set_flip_vertically_on_load(1);
            void* data = stbi_load_16("assets/textures/newport_loft.hdr", &width, &height, &channels, STBI_rgb_alpha);
            stbi_set_flip_vertically_on_load(0);
            u32 mip_levels_hdr = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

            daxa::ImageId hdr_image = device.create_image({
                .dimensions = 2,
                .format = daxa::Format::R16G16B16A16_UNORM,
                .aspect = daxa::ImageAspectFlagBits::COLOR,
                .size = { static_cast<u32>(width), static_cast<u32>(height), 1 },
                .mip_level_count = mip_levels_hdr,
                .array_layer_count = 1,
                .sample_count = 1,
                .usage = daxa::ImageUsageFlagBits::SHADER_READ_ONLY | daxa::ImageUsageFlagBits::TRANSFER_SRC | daxa::ImageUsageFlagBits::TRANSFER_DST,
                .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY
            });

            daxa::BufferId staging_buffer = device.create_buffer({
                .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .size = static_cast<u32>(width * height * sizeof(u16) * 4),
            });

            auto staging_buffer_ptr = device.get_host_address_as<u8>(staging_buffer);
            std::memcpy(staging_buffer_ptr, data, width * height * sizeof(u16) * 4);

            stbi_image_free(data);

            {
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
                    .image_slice = {
                        .base_mip_level = 0,
                        .level_count = mip_levels_hdr,
                        .base_array_layer = 0,
                        .layer_count = 1
                    },
                    .image_id = hdr_image,
                });
                cmd_list.copy_buffer_to_image({
                    .buffer = staging_buffer,
                    .buffer_offset = 0,
                    .image = hdr_image,
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

                auto image_info = device.info_image(hdr_image);
                Texture::generate_mipmaps_s(cmd_list, image_info, hdr_image);

                cmd_list.complete();
                device.submit_commands({
                    .command_lists = {std::move(cmd_list)},
                });
            }
            device.wait_idle();
            device.destroy_buffer(staging_buffer);

            daxa::SamplerId hdr_sampler = device.create_sampler({
                .magnification_filter = daxa::Filter::LINEAR,
                .minification_filter = daxa::Filter::LINEAR,
                .mipmap_filter = daxa::Filter::LINEAR,
                .address_mode_u = daxa::SamplerAddressMode::REPEAT,
                .address_mode_v = daxa::SamplerAddressMode::REPEAT,
                .address_mode_w = daxa::SamplerAddressMode::REPEAT,
                .mip_lod_bias = 0.0f,
                .enable_anisotropy = false,
                .max_anisotropy = 0.0f,
                .enable_compare = false,
                .compare_op = daxa::CompareOp::ALWAYS,
                .min_lod = 0.0f,
                .max_lod = static_cast<f32>(env_map_mip_levels),
                .enable_unnormalized_coordinates = false,
            });


            daxa::ImageId offscreen_image = device.create_image({
                .dimensions = 2,
                .format = daxa::Format::R32G32B32A32_SFLOAT,
                .size = { 512, 512, 1 },
                .mip_level_count = 1,
                .array_layer_count = 1, 
                .sample_count = 1,
                .usage = daxa::ImageUsageFlagBits::TRANSFER_SRC | daxa::ImageUsageFlagBits::COLOR_ATTACHMENT,
                .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY
            });

            struct PushData {
                u64 face_buffer;
                TextureId hdr;
                glm::mat4 mvp;
            };

            daxa::RasterPipeline equirectangular_to_cubemap_pipeline = pipeline_compiler.create_raster_pipeline({
                .vertex_shader_info = {.source = daxa::ShaderFile{"equirectangular_to_cubemap.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_VERT"}}}},
                .fragment_shader_info = {.source = daxa::ShaderFile{"equirectangular_to_cubemap.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_FRAG"}}}},
                .color_attachments = {{.format = daxa::Format::R32G32B32A32_SFLOAT, .blend = {.blend_enable = true, .src_color_blend_factor = daxa::BlendFactor::SRC_ALPHA, .dst_color_blend_factor = daxa::BlendFactor::ONE_MINUS_SRC_ALPHA}}},
                .raster = {
                    .polygon_mode = daxa::PolygonMode::FILL,
                },
                .push_constant_size = sizeof(PushData)
            }).value();

            std::vector<glm::mat4> matrices = {
                    // POSITIVE_X
                    glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
                    // NEGATIVE_X
                    glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
                    // POSITIVE_Y
                    glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
                    // NEGATIVE_Y
                    glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
                    // POSITIVE_Z
                    glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
                    // NEGATIVE_Z
                    glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            };

            daxa::CommandList cmd_list = device.create_command_list({});

            cmd_list.pipeline_barrier_image_transition({
                .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
                .waiting_pipeline_access = daxa::AccessConsts::COLOR_ATTACHMENT_OUTPUT_WRITE,
                .before_layout = daxa::ImageLayout::UNDEFINED,
                .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                .image_slice = {
                    .base_mip_level = 0,
                    .level_count = 1,
                    .base_array_layer = 0,
                    .layer_count = 1
                },
                .image_id = offscreen_image,
            });

            cmd_list.pipeline_barrier_image_transition({
                .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
                .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
                .before_layout = daxa::ImageLayout::UNDEFINED,
                .after_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                .image_slice = {
                    .base_mip_level = 0,
                    .level_count = env_map_mip_levels,
                    .base_array_layer = 0,
                    .layer_count = 6
                },
                .image_id = env_map_image,
            });

            for(u32 j = 0; j < env_map_mip_levels; j++) {
                for(u32 i = 0; i < 6; i++) {
                    cmd_list.begin_renderpass({
                        .color_attachments = {{
                            .image_view = offscreen_image.default_view(),
                            .load_op = daxa::AttachmentLoadOp::CLEAR,
                            .clear_value = std::array<f32, 4>{ 1.0f, 0.0f, 0.0f, 0.0f },
                        }},
                        .render_area = {.x = 0, .y = 0, .width = 512, .height = 512},
                    });
                    cmd_list.set_viewport({
                        .x = 0.0f,
                        .y = 0.0f,
                        .width = static_cast<f32>(512.0 * std::pow(0.5f, j)),
                        .height = static_cast<f32>(512.0 * std::pow(0.5f, j)),
                        .min_depth = 0.0f,
                        .max_depth = 1.0f 
                    });

                    cmd_list.set_pipeline(equirectangular_to_cubemap_pipeline);
                    cmd_list.push_constant(PushData{
                        .face_buffer = cube_model->vertex_buffer_address,
                        .hdr = {
                            .image_view_id = { hdr_image },
                            .sampler_id = hdr_sampler
                        },
                        .mvp = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 512.0f) * matrices[i]
                    });
                    cube_model->bind_index_buffer(cmd_list);
                    cube_model->draw(cmd_list);

                    cmd_list.end_renderpass();
                    cmd_list.pipeline_barrier_image_transition({
                        .awaited_pipeline_access = daxa::AccessConsts::COLOR_ATTACHMENT_OUTPUT_WRITE,
                        .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_READ,
                        .before_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                        .after_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                        .image_slice = {
                            .base_mip_level = 0,
                            .level_count = 1,
                            .base_array_layer = 0,
                            .layer_count = 1
                        },
                        .image_id = offscreen_image,
                    });

                    cmd_list.copy_image_to_image({
                        .src_image = offscreen_image,
                        .src_image_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                        .dst_image = env_map_image,
                        .dst_image_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                        .src_slice = {
                            .image_aspect = daxa::ImageAspectFlagBits::COLOR,
                            .mip_level = 0,
                            .base_array_layer = 0,
                            .layer_count = 1
                        },
                        .src_offset = { 0, 0, 0 },
                        .dst_slice = {
                            .image_aspect = daxa::ImageAspectFlagBits::COLOR,
                            .mip_level = j,
                            .base_array_layer = i,
                            .layer_count = 1
                        },
                        .dst_offset = { 0, 0, 0 },
                        .extent = {
                            .x = static_cast<u32>(512.0 * std::pow(0.5f, j)),
                            .y = static_cast<u32>(512.0 * std::pow(0.5f, j)),
                            .z = 1
                        }
                    });

                    cmd_list.pipeline_barrier_image_transition({
                        .awaited_pipeline_access = daxa::AccessConsts::TRANSFER_READ,
                        .waiting_pipeline_access = daxa::AccessConsts::COLOR_ATTACHMENT_OUTPUT_WRITE,
                        .before_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                        .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                        .image_slice = {
                            .base_mip_level = 0,
                            .level_count = 1,
                            .base_array_layer = 0,
                            .layer_count = 1
                        },
                        .image_id = offscreen_image,
                    });
                }
            }

            cmd_list.pipeline_barrier_image_transition({
                .awaited_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
                .waiting_pipeline_access = daxa::AccessConsts::FRAGMENT_SHADER_READ,
                .before_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                .after_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
                .image_slice = {
                    .base_mip_level = 0,
                    .level_count = env_map_mip_levels,
                    .base_array_layer = 0,
                    .layer_count = 6
                },
                .image_id = env_map_image,
            });

            cmd_list.complete();
            device.submit_commands({
                .command_lists = {std::move(cmd_list)}
            });
            device.wait_idle();
            device.destroy_image(hdr_image);
            device.destroy_sampler(hdr_sampler);
            device.destroy_image(offscreen_image);
        }

        const u32 irradiance_cube_mip_levels = static_cast<uint32_t>(std::floor(std::log2(32))) + 1;

        irradiance_cube_image = device.create_image({
            .dimensions = 2,
            .format = daxa::Format::R32G32B32A32_SFLOAT,
            .size = { 32, 32, 1 },
            .mip_level_count = irradiance_cube_mip_levels,
            .array_layer_count = 6, 
            .sample_count = 1,
            .usage = daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY
        });

        daxa::ImageViewId irradiance_cube_image_view = device.create_image_view({
            .type = daxa::ImageViewType::CUBE,
            .format = daxa::Format::R32G32B32A32_SFLOAT,
            .image = env_map_image,
            .slice = {
                .image_aspect = daxa::ImageAspectFlagBits::COLOR,
                .base_mip_level = 0,
                .level_count = irradiance_cube_mip_levels,
                .base_array_layer = 0,
                .layer_count = 6

            }
        });

        daxa::SamplerId irradiance_cube_sampler = device.create_sampler({
            .magnification_filter = daxa::Filter::LINEAR,
            .minification_filter = daxa::Filter::LINEAR,
            .mipmap_filter = daxa::Filter::LINEAR,
            .address_mode_u = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
            .address_mode_v = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
            .address_mode_w = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
            .min_lod = 0.0f,
            .max_lod = static_cast<f32>(irradiance_cube_mip_levels),
        });

        {
            daxa::ImageId offscreen_image = device.create_image({
                .dimensions = 2,
                .format = daxa::Format::R32G32B32A32_SFLOAT,
                .size = { 32, 32, 1 },
                .mip_level_count = 1,
                .array_layer_count = 1, 
                .sample_count = 1,
                .usage = daxa::ImageUsageFlagBits::TRANSFER_SRC | daxa::ImageUsageFlagBits::COLOR_ATTACHMENT,
                .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY
            });

            struct PushData {
                u64 face_buffer;
                TextureId env_map;
                f32 delta_phi = (2.0f * float(M_PI)) / 180.0f;
                f32 delta_theta = (0.5f * float(M_PI)) / 64.0f;
                glm::mat4 mvp;
            };

            daxa::RasterPipeline irradiance_cube_pipeline = pipeline_compiler.create_raster_pipeline({
                .vertex_shader_info = {.source = daxa::ShaderFile{"irradiance_cube.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_VERT"}}}},
                .fragment_shader_info = {.source = daxa::ShaderFile{"irradiance_cube.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_FRAG"}}}},
                .color_attachments = {{.format = daxa::Format::R32G32B32A32_SFLOAT, .blend = {.blend_enable = true, .src_color_blend_factor = daxa::BlendFactor::SRC_ALPHA, .dst_color_blend_factor = daxa::BlendFactor::ONE_MINUS_SRC_ALPHA}}},
                .raster = {
                    .polygon_mode = daxa::PolygonMode::FILL,
                },
                .push_constant_size = sizeof(PushData)
            }).value();


            std::vector<glm::mat4> matrices = {
                    // POSITIVE_X
                    glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
                    // NEGATIVE_X
                    glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
                    // POSITIVE_Y
                    glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
                    // NEGATIVE_Y
                    glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
                    // POSITIVE_Z
                    glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
                    // NEGATIVE_Z
                    glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            };

            daxa::CommandList cmd_list = device.create_command_list({});

            cmd_list.pipeline_barrier_image_transition({
                .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
                .waiting_pipeline_access = daxa::AccessConsts::COLOR_ATTACHMENT_OUTPUT_WRITE,
                .before_layout = daxa::ImageLayout::UNDEFINED,
                .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                .image_slice = {
                    .base_mip_level = 0,
                    .level_count = 1,
                    .base_array_layer = 0,
                    .layer_count = 1
                },
                .image_id = offscreen_image,
            });

            cmd_list.pipeline_barrier_image_transition({
                .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
                .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
                .before_layout = daxa::ImageLayout::UNDEFINED,
                .after_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                .image_slice = {
                    .base_mip_level = 0,
                    .level_count = irradiance_cube_mip_levels,
                    .base_array_layer = 0,
                    .layer_count = 6
                },
                .image_id = irradiance_cube_image,
            });

            for(u32 j = 0; j < irradiance_cube_mip_levels; j++) {
                for(u32 i = 0; i < 6; i++) {
                    cmd_list.begin_renderpass({
                        .color_attachments = {{
                            .image_view = offscreen_image.default_view(),
                            .load_op = daxa::AttachmentLoadOp::CLEAR,
                            .clear_value = std::array<f32, 4>{ 1.0f, 0.0f, 0.0f, 0.0f },
                        }},
                        .render_area = {.x = 0, .y = 0, .width = 32, .height = 32},
                    });

                    cmd_list.set_viewport({
                        .x = 0.0f,
                        .y = 0.0f,
                        .width = static_cast<f32>(32.0 * std::pow(0.5f, j)),
                        .height = static_cast<f32>(32.0 * std::pow(0.5f, j)),
                        .min_depth = 0.0f,
                        .max_depth = 1.0f 
                    });

                    cmd_list.set_pipeline(irradiance_cube_pipeline);
                    cmd_list.push_constant(PushData{
                        .face_buffer = cube_model->vertex_buffer_address,
                        .env_map = {
                            .image_view_id = env_map_image_view,
                            .sampler_id = env_map_sampler
                        },
                        .delta_phi = (2.0f * float(M_PI)) / 180.0f,
                        .delta_theta = (0.5f * float(M_PI)) / 64.0f,
                        .mvp = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 512.0f) * matrices[i]
                    });
                    cube_model->bind_index_buffer(cmd_list);
                    cube_model->draw(cmd_list);

                    cmd_list.end_renderpass();
                    cmd_list.pipeline_barrier_image_transition({
                        .awaited_pipeline_access = daxa::AccessConsts::COLOR_ATTACHMENT_OUTPUT_WRITE,
                        .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_READ,
                        .before_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                        .after_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                        .image_slice = {
                            .base_mip_level = 0,
                            .level_count = 1,
                            .base_array_layer = 0,
                            .layer_count = 1
                        },
                        .image_id = offscreen_image,
                    });

                    cmd_list.copy_image_to_image({
                        .src_image = offscreen_image,
                        .src_image_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                        .dst_image = irradiance_cube_image,
                        .dst_image_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                        .src_slice = {
                            .image_aspect = daxa::ImageAspectFlagBits::COLOR,
                            .mip_level = 0,
                            .base_array_layer = 0,
                            .layer_count = 1
                        },
                        .src_offset = { 0, 0, 0 },
                        .dst_slice = {
                            .image_aspect = daxa::ImageAspectFlagBits::COLOR,
                            .mip_level = j,
                            .base_array_layer = i,
                            .layer_count = 1
                        },
                        .dst_offset = { 0, 0, 0 },
                        .extent = {
                            .x = static_cast<u32>(32.0 * std::pow(0.5f, j)),
                            .y = static_cast<u32>(32.0 * std::pow(0.5f, j)),
                            .z = 1
                        }
                    });

                    cmd_list.pipeline_barrier_image_transition({
                        .awaited_pipeline_access = daxa::AccessConsts::TRANSFER_READ,
                        .waiting_pipeline_access = daxa::AccessConsts::COLOR_ATTACHMENT_OUTPUT_WRITE,
                        .before_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                        .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                        .image_slice = {
                            .base_mip_level = 0,
                            .level_count = 1,
                            .base_array_layer = 0,
                            .layer_count = 1
                        },
                        .image_id = offscreen_image,
                    });
                }
            }

            cmd_list.pipeline_barrier_image_transition({
                .awaited_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
                .waiting_pipeline_access = daxa::AccessConsts::FRAGMENT_SHADER_READ,
                .before_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                .after_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
                .image_slice = {
                    .base_mip_level = 0,
                    .level_count = irradiance_cube_mip_levels,
                    .base_array_layer = 0,
                    .layer_count = 6
                },
                .image_id = irradiance_cube_image,
            });

            cmd_list.complete();
            device.submit_commands({
                .command_lists = {std::move(cmd_list)}
            });
            device.wait_idle();
            device.destroy_image(offscreen_image);
        }

        const u32 prefiltered_cube_mip_levels = static_cast<uint32_t>(std::floor(std::log2(512))) + 1;

        prefiltered_cube_image = device.create_image({
            .dimensions = 2,
            .format = daxa::Format::R32G32B32A32_SFLOAT,
            .size = { 512, 512, 1 },
            .mip_level_count = prefiltered_cube_mip_levels,
            .array_layer_count = 6, 
            .sample_count = 1,
            .usage = daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY
        });

        daxa::ImageViewId prefiltered_cube_image_view = device.create_image_view({
            .type = daxa::ImageViewType::CUBE,
            .format = daxa::Format::R32G32B32A32_SFLOAT,
            .image = env_map_image,
            .slice = {
                .image_aspect = daxa::ImageAspectFlagBits::COLOR,
                .base_mip_level = 0,
                .level_count = prefiltered_cube_mip_levels,
                .base_array_layer = 0,
                .layer_count = 6

            }
        });

        daxa::SamplerId prefiltered_cube_sampler = device.create_sampler({
            .magnification_filter = daxa::Filter::LINEAR,
            .minification_filter = daxa::Filter::LINEAR,
            .mipmap_filter = daxa::Filter::LINEAR,
            .address_mode_u = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
            .address_mode_v = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
            .address_mode_w = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
            .min_lod = 0.0f,
            .max_lod = static_cast<f32>(prefiltered_cube_mip_levels),
        });

        {
            daxa::ImageId offscreen_image = device.create_image({
                .dimensions = 2,
                .format = daxa::Format::R32G32B32A32_SFLOAT,
                .size = { 512, 512, 1 },
                .mip_level_count = 1,
                .array_layer_count = 1, 
                .sample_count = 1,
                .usage = daxa::ImageUsageFlagBits::TRANSFER_SRC | daxa::ImageUsageFlagBits::COLOR_ATTACHMENT,
                .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY
            });

            struct PushData {
                u64 face_buffer;
                TextureId env_map;
                f32 roughness;
                glm::mat4 mvp;
            };

            daxa::RasterPipeline prefilter_env_pipeline = pipeline_compiler.create_raster_pipeline({
                .vertex_shader_info = {.source = daxa::ShaderFile{"prefilter_env.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_VERT"}}}},
                .fragment_shader_info = {.source = daxa::ShaderFile{"prefilter_env.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_FRAG"}}}},
                .color_attachments = {{.format = daxa::Format::R32G32B32A32_SFLOAT, .blend = {.blend_enable = true, .src_color_blend_factor = daxa::BlendFactor::SRC_ALPHA, .dst_color_blend_factor = daxa::BlendFactor::ONE_MINUS_SRC_ALPHA}}},
                .raster = {
                    .polygon_mode = daxa::PolygonMode::FILL,
                },
                .push_constant_size = sizeof(PushData)
            }).value();

            std::vector<glm::mat4> matrices = {
                    // POSITIVE_X
                    glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
                    // NEGATIVE_X
                    glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
                    // POSITIVE_Y
                    glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
                    // NEGATIVE_Y
                    glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
                    // POSITIVE_Z
                    glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
                    // NEGATIVE_Z
                    glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            };

            daxa::CommandList cmd_list = device.create_command_list({});

            cmd_list.pipeline_barrier_image_transition({
                .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
                .waiting_pipeline_access = daxa::AccessConsts::COLOR_ATTACHMENT_OUTPUT_WRITE,
                .before_layout = daxa::ImageLayout::UNDEFINED,
                .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                .image_slice = {
                    .base_mip_level = 0,
                    .level_count = 1,
                    .base_array_layer = 0,
                    .layer_count = 1
                },
                .image_id = offscreen_image,
            });

            cmd_list.pipeline_barrier_image_transition({
                .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
                .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
                .before_layout = daxa::ImageLayout::UNDEFINED,
                .after_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                .image_slice = {
                    .base_mip_level = 0,
                    .level_count = prefiltered_cube_mip_levels,
                    .base_array_layer = 0,
                    .layer_count = 6
                },
                .image_id = prefiltered_cube_image,
            });


            for(u32 j = 0; j < prefiltered_cube_mip_levels; j++) {
                for(u32 i = 0; i < 6; i++) {
                    cmd_list.begin_renderpass({
                        .color_attachments = {{
                            .image_view = offscreen_image.default_view(),
                            .load_op = daxa::AttachmentLoadOp::CLEAR,
                            .clear_value = std::array<f32, 4>{ 1.0f, 0.0f, 0.0f, 0.0f },
                        }},
                        .render_area = { .x = 0, .y = 0, .width = 512, .height = 512 },
                    });

                    cmd_list.set_viewport({
                        .x = 0.0f,
                        .y = 0.0f,
                        .width = static_cast<f32>(512.0 * std::pow(0.5f, j)),
                        .height = static_cast<f32>(512.0 * std::pow(0.5f, j)),
                        .min_depth = 0.0f,
                        .max_depth = 1.0f 
                    });

                    cmd_list.set_pipeline(prefilter_env_pipeline);
                    cmd_list.push_constant(PushData{
                        .face_buffer = cube_model->vertex_buffer_address,
                        .env_map = {
                            .image_view_id = env_map_image_view,
                            .sampler_id = env_map_sampler
                        },
                        .roughness = (float)j / (float)(prefiltered_cube_mip_levels - 1),
                        .mvp = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 512.0f) * matrices[i]
                    });
                    cube_model->bind_index_buffer(cmd_list);
                    cube_model->draw(cmd_list);

                    cmd_list.end_renderpass();
                    cmd_list.pipeline_barrier_image_transition({
                        .awaited_pipeline_access = daxa::AccessConsts::COLOR_ATTACHMENT_OUTPUT_WRITE,
                        .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_READ,
                        .before_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                        .after_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                        .image_slice = {
                            .base_mip_level = 0,
                            .level_count = 1,
                            .base_array_layer = 0,
                            .layer_count = 1
                        },
                        .image_id = offscreen_image,
                    });

                    cmd_list.copy_image_to_image({
                        .src_image = offscreen_image,
                        .src_image_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                        .dst_image = prefiltered_cube_image,
                        .dst_image_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                        .src_slice = {
                            .image_aspect = daxa::ImageAspectFlagBits::COLOR,
                            .mip_level = 0,
                            .base_array_layer = 0,
                            .layer_count = 1
                        },
                        .src_offset = { 0, 0, 0 },
                        .dst_slice = {
                            .image_aspect = daxa::ImageAspectFlagBits::COLOR,
                            .mip_level = j,
                            .base_array_layer = i,
                            .layer_count = 1
                        },
                        .dst_offset = { 0, 0, 0 },
                        .extent = {
                            .x = static_cast<u32>(512.0 * std::pow(0.5f, j)),
                            .y = static_cast<u32>(512.0 * std::pow(0.5f, j)),
                            .z = 1
                        }
                    });

                    cmd_list.pipeline_barrier_image_transition({
                        .awaited_pipeline_access = daxa::AccessConsts::TRANSFER_READ,
                        .waiting_pipeline_access = daxa::AccessConsts::COLOR_ATTACHMENT_OUTPUT_WRITE,
                        .before_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                        .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                        .image_slice = {
                            .base_mip_level = 0,
                            .level_count = 1,
                            .base_array_layer = 0,
                            .layer_count = 1
                        },
                        .image_id = offscreen_image,
                    });
                }
            }

            cmd_list.pipeline_barrier_image_transition({
                .awaited_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
                .waiting_pipeline_access = daxa::AccessConsts::FRAGMENT_SHADER_READ,
                .before_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                .after_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
                .image_slice = {
                    .base_mip_level = 0,
                    .level_count = prefiltered_cube_mip_levels,
                    .base_array_layer = 0,
                    .layer_count = 6
                },
                .image_id = prefiltered_cube_image,
            });

            cmd_list.complete();
            device.submit_commands({
                .command_lists = {std::move(cmd_list)}
            });
            device.wait_idle();
            device.destroy_image(offscreen_image);
        }

        BRDFLUT = {
            .image_view_id = { BRDFLUT_image },
            .sampler_id = BRDFLUT_sampler
        };
        env_map = {
            .image_view_id = env_map_image_view,
            .sampler_id = env_map_sampler
        };
        irradiance_cube = {
            .image_view_id = irradiance_cube_image_view,
            .sampler_id = irradiance_cube_sampler
        };
        prefiltered_cube = {
            .image_view_id = prefiltered_cube_image_view,
            .sampler_id = prefiltered_cube_sampler
        };
    }

    void IBLRenderer::draw(daxa::CommandList& cmd_list, const glm::mat4& proj, const glm::mat4& view) {
        glm::mat4 temp_mvp = proj * glm::mat4(glm::mat3(view));
        cmd_list.set_pipeline(skybox_pipeline);
        cmd_list.push_constant(SkyboxDrawPush{
            .mvp = *reinterpret_cast<const f32mat4x4*>(&temp_mvp),
            .face_buffer = cube_model->vertex_buffer_address,
            .env_map = env_map,
        });
        cube_model->bind_index_buffer(cmd_list);
        cube_model->draw(cmd_list);
    }
    IBLRenderer::~IBLRenderer() {
        device.destroy_image({ BRDFLUT.image_view_id });
        device.destroy_sampler(BRDFLUT.sampler_id);
        device.destroy_image(env_map_image);
        device.destroy_sampler(env_map.sampler_id);
        device.destroy_image_view(env_map.image_view_id);
        device.destroy_image(irradiance_cube_image);
        device.destroy_sampler(irradiance_cube.sampler_id);
        device.destroy_image_view(irradiance_cube.image_view_id);
        device.destroy_image(prefiltered_cube_image);
        device.destroy_sampler(prefiltered_cube.sampler_id);
        device.destroy_image_view(prefiltered_cube.image_view_id);
    }
}