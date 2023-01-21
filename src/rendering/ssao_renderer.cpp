#include "ssao_renderer.hpp"
#include "../../shaders/shared.inl"

namespace dare {
    SSAORenderer::SSAORenderer(daxa::PipelineManager& pipeline_manager, daxa::Device device, const glm::ivec2& half_size) : device{device}, half_size{half_size} {
        this->ssao_generation_pipeline = pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = {
                .source = daxa::ShaderFile{"ssao_generation.glsl"}, 
                .compile_options = { .defines = { daxa::ShaderDefine{"DRAW_VERT"} } }
            },
            .fragment_shader_info = {
                .source = daxa::ShaderFile{"ssao_generation.glsl"},
                .compile_options = { .defines = { daxa::ShaderDefine{"DRAW_FRAG"} } }
            },
            .color_attachments = { 
                { .format = daxa::Format::R8_UNORM, }
            },
            .raster = {
                .polygon_mode = daxa::PolygonMode::FILL,
            },
            .push_constant_size = sizeof(SSAOGenerationPush),
            .debug_name = "ssao_generation_code",
        }).value();

        this->ssao_blur_pipeline = pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = {
                .source = daxa::ShaderFile{"ssao_blur.glsl"},
                .compile_options = { .defines = { daxa::ShaderDefine{"DRAW_VERT"} } }
            },
            .fragment_shader_info = {
                .source = daxa::ShaderFile{"ssao_blur.glsl"},
                .compile_options = { .defines = { daxa::ShaderDefine{"DRAW_FRAG"} } }
            },
            .color_attachments = {
                { .format = daxa::Format::R8_UNORM, }
            },
            .raster = {
                .polygon_mode = daxa::PolygonMode::FILL,
            },
            .push_constant_size = sizeof(SSAOBlurPush),
            .debug_name = "ssao_blur_pipeline",
        }).value();
        
        this->ssao_image = this->device.create_image({
            .format = daxa::Format::R8_UNORM,
            .aspect = daxa::ImageAspectFlagBits::COLOR,
            .size = { static_cast<u32>(half_size.x), static_cast<u32>(half_size.y), 1 },
            .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
        });

        this->ssao_blur_image = this->device.create_image({
            .format = daxa::Format::R8_UNORM,
            .aspect = daxa::ImageAspectFlagBits::COLOR,
            .size = { static_cast<u32>(half_size.x), static_cast<u32>(half_size.y), 1 },
            .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
        });
    }

    SSAORenderer::~SSAORenderer() {
        device.destroy_image(this->ssao_image);
        device.destroy_image(this->ssao_blur_image);
    }

    void SSAORenderer::render(daxa::CommandList& cmd_list, daxa::ImageId& normal_image, daxa::ImageId depth_image, daxa::BufferDeviceAddress camera_buffer_address, daxa::SamplerId& sampler) {
        cmd_list.begin_renderpass({
            .color_attachments = {
                {
                    .image_view = this->ssao_image.default_view(),
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<f32, 4>{1.0f, 0.0f, 0.0f, 1.0f},
                },
            },
            .render_area = {.x = 0, .y = 0, .width = static_cast<u32>(static_cast<u32>(half_size.x)), .height = static_cast<u32>(static_cast<u32>(half_size.y))},
        });
        cmd_list.set_pipeline(*ssao_generation_pipeline);
        cmd_list.push_constant(SSAOGenerationPush {
            .normal = { .image_view_id = normal_image.default_view(), .sampler_id = sampler },
            .depth = { .image_view_id = depth_image.default_view(), .sampler_id = sampler },
            .camera_buffer = camera_buffer_address
        });
        cmd_list.draw({ .vertex_count = 3 });
        cmd_list.end_renderpass();
    // SSAO blur
        cmd_list.begin_renderpass({
            .color_attachments = {
                {
                    .image_view = this->ssao_blur_image.default_view(),
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<f32, 4>{0.0f, 0.0f, 0.0f, 1.0f},
                },
            },
            .render_area = {.x = 0, .y = 0, .width = static_cast<u32>(static_cast<u32>(half_size.x)), .height = static_cast<u32>(static_cast<u32>(half_size.y))},
        });
        cmd_list.set_pipeline(*ssao_blur_pipeline);
        cmd_list.push_constant(SSAOBlurPush {
            .ssao = { .image_view_id = ssao_image.default_view(), .sampler_id = sampler },
        });
        cmd_list.draw({ .vertex_count = 3 });
        cmd_list.end_renderpass();
    }

    void SSAORenderer::resize(const glm::ivec2& size) {
        half_size = size;

        device.destroy_image(ssao_image);
        this->ssao_image = this->device.create_image({
            .format = daxa::Format::R8_UNORM,
            .aspect = daxa::ImageAspectFlagBits::COLOR,
            .size = { static_cast<u32>(half_size.x), static_cast<u32>(half_size.y), 1 },
            .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
        });

        device.destroy_image(ssao_blur_image);
        this->ssao_blur_image = this->device.create_image({
            .format = daxa::Format::R8_UNORM,
            .aspect = daxa::ImageAspectFlagBits::COLOR,
            .size = { static_cast<u32>(half_size.x), static_cast<u32>(half_size.y), 1 },
            .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
        });
    }
}