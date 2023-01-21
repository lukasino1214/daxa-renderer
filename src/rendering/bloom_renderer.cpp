#include "bloom_renderer.hpp"
#include "../../shaders/shared.inl"
#include <iostream>

namespace dare {
    BloomRenderer::BloomRenderer(daxa::PipelineManager& pipeline_manager, daxa::Device device, const glm::ivec2& window_size) : device{device}, window_size{window_size} {
        down_sample_pipeline = pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = {.source = daxa::ShaderFile{"down_sample.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_VERT"}}}},
            .fragment_shader_info = {.source = daxa::ShaderFile{"down_sample.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_FRAG"}}}},
            .color_attachments = {
                { .format = daxa::Format::R16G16B16A16_SFLOAT },
            },
            .raster = {
                .face_culling = daxa::FaceCullFlagBits::NONE
            },
            .push_constant_size = sizeof(BloomPush),
            .debug_name = "raster_pipeline",
        }).value();

        up_sample_pipeline = pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = {.source = daxa::ShaderFile{"up_sample.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_VERT"}}}},
            .fragment_shader_info = {.source = daxa::ShaderFile{"up_sample.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_FRAG"}}}},
            .color_attachments = {
                { .format = daxa::Format::R16G16B16A16_SFLOAT },
            },
            .raster = {
                .face_culling = daxa::FaceCullFlagBits::NONE
            },
            .push_constant_size = sizeof(BloomPush),
            .debug_name = "raster_pipeline",
        }).value();

        glm::vec2 mip_size(static_cast<f32>(window_size.x), static_cast<f32>(window_size.y));
        glm::ivec2 mip_int_size = window_size;

        for(u32 i = 0; i < 2; i++) {
            mip_int_size /= 2;
            mip_size *= 0.5f;

            BloomMip mip;
            mip.int_size = mip_int_size;
            mip.size = mip_size;
            mip.texture = device.create_image({
                .format = daxa::Format::R16G16B16A16_SFLOAT,
                .aspect = daxa::ImageAspectFlagBits::COLOR,
                .size = { static_cast<u32>(mip_int_size.x), static_cast<u32>(mip_int_size.y), 1 },
                .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
            });

            mip_chain.push_back(std::move(mip));
        }
    }

    BloomRenderer::~BloomRenderer() {
        for(u32 i = 0; i < mip_chain.size(); i++) {
            device.destroy_image(mip_chain[i].texture);
        }
    }

    void BloomRenderer::resize(const glm::ivec2& size) {
        window_size = size;
        for(u32 i = 0; i < mip_chain.size(); i++) {
            device.destroy_image(mip_chain[i].texture);
        }

        glm::vec2 mip_size(static_cast<f32>(window_size.x), static_cast<f32>(window_size.y));
        glm::ivec2 mip_int_size = window_size;
        
        for(u32 i = 0; i < 2; i++) {
            mip_int_size /= 2;
            mip_size *= 0.5f;
            auto& mip = mip_chain[i];
            mip.int_size = mip_int_size;
            mip.size = mip_size;
            mip.texture = device.create_image({
                .format = daxa::Format::R16G16B16A16_SFLOAT,
                .aspect = daxa::ImageAspectFlagBits::COLOR,
                .size = { static_cast<u32>(mip_int_size.x), static_cast<u32>(mip_int_size.y), 1 },
                .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
            });
        }
    }

    void BloomRenderer::render(daxa::CommandList& cmd_list, daxa::ImageId& src_image, daxa::SamplerId& sampler) {
        render_downsamples(cmd_list, src_image, sampler);
        render_upsamples(cmd_list, src_image, sampler);
    }

    void BloomRenderer::render_downsamples(daxa::CommandList& cmd_list, daxa::ImageId& src_image, daxa::SamplerId& sampler) {
    cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
            .before_layout = daxa::ImageLayout::UNDEFINED,
            .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
            .image_id = mip_chain[0].texture
        });

        cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::READ,
            .before_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
            .after_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
            .image_id = src_image
        });

        cmd_list.begin_renderpass({
            .color_attachments = {
                {
                    .image_view = this->mip_chain[0].texture.default_view(),
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<f32, 4>{0.0f, 0.0f, 0.0f, 1.0f},
                },
            },
            .render_area = {.x = 0, .y = 0, .width = static_cast<u32>(mip_chain[0].int_size.x), .height = static_cast<u32>(mip_chain[0].int_size.y)},
        });
        cmd_list.set_pipeline(*down_sample_pipeline);
        cmd_list.push_constant(BloomPush {
            .src_texture = { .image_view_id = src_image.default_view(), .sampler_id = sampler },
            .src_resolution = { static_cast<f32>(window_size.x), static_cast<f32>(window_size.y) },
            .filter_radius = 0.0f
        });
        cmd_list.draw({ .vertex_count = 3 });
        cmd_list.end_renderpass();

        for(u32 i = 0; i < mip_chain.size() - 1; i++) {
            cmd_list.pipeline_barrier_image_transition({
                .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
                .before_layout = daxa::ImageLayout::UNDEFINED,
                .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                .image_id = mip_chain[i+1].texture
            });

            cmd_list.pipeline_barrier_image_transition({
                .waiting_pipeline_access = daxa::AccessConsts::READ,
                .before_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                .after_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
                .image_id = mip_chain[i].texture
            });

            cmd_list.begin_renderpass({
                .color_attachments = {
                    {
                        .image_view = this->mip_chain[i+1].texture.default_view(),
                        .load_op = daxa::AttachmentLoadOp::CLEAR,
                        .clear_value = std::array<f32, 4>{0.0f, 0.0f, 0.0f, 1.0f},
                    },
                },
                .render_area = {.x = 0, .y = 0, .width = static_cast<u32>(mip_chain[i+1].int_size.x), .height = static_cast<u32>(mip_chain[i+1].int_size.y)},
            });
            cmd_list.set_pipeline(*down_sample_pipeline);
            cmd_list.push_constant(BloomPush {
                .src_texture = { .image_view_id = mip_chain[i].texture.default_view(), .sampler_id = sampler },
                .src_resolution = { static_cast<f32>(mip_chain[i].int_size.x ), static_cast<f32>(mip_chain[i].int_size.y) },
                .filter_radius = 0.0f
            });
            cmd_list.draw({ .vertex_count = 3 });
            cmd_list.end_renderpass();
        }
    }

    void BloomRenderer::render_upsamples(daxa::CommandList& cmd_list, daxa::ImageId& src_image, daxa::SamplerId& sampler) {
        for(u32 i = mip_chain.size() - 1; i > 0; i--) {
            cmd_list.pipeline_barrier_image_transition({
                .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
                .before_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
                .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                .image_id = mip_chain[i-1].texture
            });

            cmd_list.pipeline_barrier_image_transition({
                .waiting_pipeline_access = daxa::AccessConsts::READ,
                .before_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                .after_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
                .image_id = mip_chain[i].texture
            });

            cmd_list.begin_renderpass({
                .color_attachments = {
                    {
                        .image_view = mip_chain[i-1].texture.default_view(),
                        .load_op = daxa::AttachmentLoadOp::CLEAR,
                        .clear_value = std::array<f32, 4>{0.0f, 0.0f, 0.0f, 1.0f},
                    },
                },
                .render_area = {.x = 0, .y = 0, .width = static_cast<u32>(mip_chain[i-1].int_size.x), .height = static_cast<u32>(mip_chain[i-1].int_size.y)},
            });
            cmd_list.set_pipeline(*up_sample_pipeline);
            cmd_list.push_constant(BloomPush {
                .src_texture = { .image_view_id = mip_chain[i].texture.default_view(), .sampler_id = sampler },
                .src_resolution = { static_cast<f32>(mip_chain[i].int_size.x), static_cast<f32>(mip_chain[i].int_size.y) },
                .filter_radius = 0.005f
            });
            cmd_list.draw({ .vertex_count = 3 });
            cmd_list.end_renderpass();
        }

        cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
            .before_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
            .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
            .image_id = src_image
        });

        cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::READ,
            .before_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
            .after_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
            .image_id = mip_chain[0].texture
        });

        cmd_list.begin_renderpass({
            .color_attachments = {
                {
                    .image_view = src_image.default_view(),
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<f32, 4>{0.0f, 0.0f, 0.0f, 1.0f},
                },
            },
            .render_area = {.x = 0, .y = 0, .width = static_cast<u32>(window_size.x ), .height = static_cast<u32>(window_size.y)},
        });
        cmd_list.set_pipeline(*up_sample_pipeline);
        cmd_list.push_constant(BloomPush {
            .src_texture = { .image_view_id = mip_chain[0].texture.default_view(), .sampler_id = sampler },
            .src_resolution = { static_cast<f32>(mip_chain[0].int_size.x), static_cast<f32>(mip_chain[0].int_size.y) },
            .filter_radius = 0.005f
        });
        cmd_list.draw({ .vertex_count = 3 });
        cmd_list.end_renderpass();
    }
}