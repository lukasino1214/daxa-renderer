#include "bloom.hpp"
#include "../shaders/shared.inl"

BloomMip::BloomMip(daxa::Device device, const glm::vec2& size, const glm::ivec2& int_size) : device{device}, size{size}, int_size{int_size} {
    texture = device.create_image({
        .format = daxa::Format::R16G16B16A16_SFLOAT,
        .aspect = daxa::ImageAspectFlagBits::COLOR,
        .size = { static_cast<u32>(int_size.x), static_cast<u32>(int_size.y), 1 },
        .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
    });
}

BloomMip::~BloomMip() {
    device.destroy_image(texture);
}

BloomFBO::BloomFBO(daxa::Device device, const glm::ivec2& window_size, u32 mip_chain_length) : device{device} {
    glm::vec2 mip_size(static_cast<f32>(window_size.x), static_cast<f32>(window_size.y));
	glm::ivec2 mip_int_size = window_size;

    for(u32 i = 0; i < mip_chain_length; i++) {
        mip_int_size /= 2;
        mip_size *= 0.5f;
        std::cout << "created mip: " << mip_int_size.x << " " << mip_int_size.y << std::endl;
        std::unique_ptr<BloomMip> mip = std::make_unique<BloomMip>(device, mip_size, mip_int_size);
        mip_chain.push_back(std::move(mip));
    }
}

BloomFBO::~BloomFBO() {

}

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

    this->bloom_mips = std::make_unique<BloomFBO>(device, window_size, 6);
}

BloomRenderer::~BloomRenderer() {}

void BloomRenderer::render(daxa::CommandList& cmd_list, daxa::ImageId& src_image, daxa::SamplerId& sampler) {
    render_downsamples(cmd_list, src_image, sampler);
    render_upsamples(cmd_list, src_image, sampler);
}

void BloomRenderer::render_downsamples(daxa::CommandList& cmd_list, daxa::ImageId& src_image, daxa::SamplerId& sampler) {
    /*
    std::cout << "start downsample" << std::endl;
    // do here from src to 0
    std::cout << "src: " << window_size.x << " " << window_size.y << std::endl;
    std::cout << "dst: " << bloom_mips->mip_chain[0]->int_size.x << " " << bloom_mips->mip_chain[0]->int_size.y << std::endl;

    for(u32 i = 0; i < bloom_mips->mip_chain.size() - 1; i++) {
        std::cout << "src: " << bloom_mips->mip_chain[i]->int_size.x << " " << bloom_mips->mip_chain[i]->int_size.y << std::endl;
        std::cout << "dst: " << bloom_mips->mip_chain[i+1]->int_size.x << " " << bloom_mips->mip_chain[i+1]->int_size.y << std::endl;
    }
    */

    cmd_list.begin_renderpass({
        .color_attachments = {
            {
                .image_view = this->bloom_mips->mip_chain[0]->texture.default_view(),
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = std::array<f32, 4>{0.0f, 0.0f, 0.0f, 1.0f},
            },
        },
        .render_area = {.x = 0, .y = 0, .width = static_cast<u32>(bloom_mips->mip_chain[0]->int_size.x), .height = static_cast<u32>(bloom_mips->mip_chain[0]->int_size.y)},
    });
    cmd_list.set_pipeline(*down_sample_pipeline);
    cmd_list.push_constant(BloomPush {
        .src_texture = { .image_view_id = src_image.default_view(), .sampler_id = sampler },
        .src_resolution = { static_cast<f32>(window_size.x), static_cast<f32>(window_size.y) },
        .filter_radius = 0.0f
    });
    cmd_list.draw({ .vertex_count = 3 });
    cmd_list.end_renderpass();

    for(u32 i = 0; i < bloom_mips->mip_chain.size() - 1; i++) {
        cmd_list.begin_renderpass({
            .color_attachments = {
                {
                    .image_view = this->bloom_mips->mip_chain[i+1]->texture.default_view(),
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<f32, 4>{0.0f, 0.0f, 0.0f, 1.0f},
                },
            },
            .render_area = {.x = 0, .y = 0, .width = static_cast<u32>(bloom_mips->mip_chain[i+1]->int_size.x), .height = static_cast<u32>(bloom_mips->mip_chain[i+1]->int_size.y)},
        });
        cmd_list.set_pipeline(*down_sample_pipeline);
        cmd_list.push_constant(BloomPush {
            .src_texture = { .image_view_id = bloom_mips->mip_chain[i]->texture.default_view(), .sampler_id = sampler },
            .src_resolution = { static_cast<f32>(bloom_mips->mip_chain[i]->int_size.x ), static_cast<f32>(bloom_mips->mip_chain[i]->int_size.y) },
            .filter_radius = 0.0f
        });
        cmd_list.draw({ .vertex_count = 3 });
        cmd_list.end_renderpass();
    }

   // do here from src to 0
}

void BloomRenderer::render_upsamples(daxa::CommandList& cmd_list, daxa::ImageId& src_image, daxa::SamplerId& sampler) {
    /*
    std::cout << "start upsample" << std::endl;
    for(u32 i = bloom_mips->mip_chain.size() - 1; i > 0; i--) {
        std::cout << "src: " << bloom_mips->mip_chain[i]->int_size.x << " " << bloom_mips->mip_chain[i]->int_size.y << std::endl;
        std::cout << "dst: " << bloom_mips->mip_chain[i-1]->int_size.x << " " << bloom_mips->mip_chain[i-1]->int_size.y << std::endl;
    }

    // do here from 0 to src
    std::cout << "src: " << bloom_mips->mip_chain[0]->int_size.x << " " << bloom_mips->mip_chain[0]->int_size.y << std::endl;
    std::cout << "dst: " << window_size.x << " " << window_size.y << std::endl;
    */

    for(u32 i = bloom_mips->mip_chain.size() - 1; i > 0; i--) {
        cmd_list.begin_renderpass({
            .color_attachments = {
                {
                    .image_view = bloom_mips->mip_chain[i-1]->texture.default_view(),
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<f32, 4>{0.0f, 0.0f, 0.0f, 1.0f},
                },
            },
            .render_area = {.x = 0, .y = 0, .width = static_cast<u32>(bloom_mips->mip_chain[i-1]->int_size.x), .height = static_cast<u32>(bloom_mips->mip_chain[i-1]->int_size.y)},
        });
        cmd_list.set_pipeline(*up_sample_pipeline);
        cmd_list.push_constant(BloomPush {
            .src_texture = { .image_view_id = bloom_mips->mip_chain[i]->texture.default_view(), .sampler_id = sampler },
            .src_resolution = { static_cast<f32>(bloom_mips->mip_chain[i]->int_size.x), static_cast<f32>(bloom_mips->mip_chain[i]->int_size.y) },
            .filter_radius = 0.005f
        });
        cmd_list.draw({ .vertex_count = 3 });
        cmd_list.end_renderpass();
    }

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
        .src_texture = { .image_view_id = bloom_mips->mip_chain[0]->texture.default_view(), .sampler_id = sampler },
        .src_resolution = { static_cast<f32>(bloom_mips->mip_chain[0]->int_size.x), static_cast<f32>(bloom_mips->mip_chain[0]->int_size.y) },
        .filter_radius = 0.005f
    });
    cmd_list.draw({ .vertex_count = 3 });
    cmd_list.end_renderpass();
}
