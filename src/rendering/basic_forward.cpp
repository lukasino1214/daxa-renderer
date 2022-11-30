#include "basic_forward.hpp"

#include <imgui.h>

namespace dare {
    BasicForward::BasicForward(RenderContext& context) : Task(context) {
        u32 sx = static_cast<u32>(this->size.x);
        u32 sy = static_cast<u32>(this->size.y);

        this->color_image = this->context.device.create_image({
            .dimensions = 2,
            .format = this->context.swapchain.get_format(),
            .aspect = daxa::ImageAspectFlagBits::COLOR,
            .size = { sx, sy, 1 },
            .mip_level_count = 1,
            .array_layer_count = 1,
            .sample_count = 1,
            .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY
        });

        this->depth_image = this->context.device.create_image({
            .dimensions = 2,
            .format = daxa::Format::D24_UNORM_S8_UINT,
            .aspect = daxa::ImageAspectFlagBits::DEPTH | daxa::ImageAspectFlagBits::STENCIL,
            .size = { sx, sy, 1 },
            .mip_level_count = 1,
            .array_layer_count = 1,
            .sample_count = 1,
            .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT,
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY
        });

        rebuild_pipeline();
    }

    BasicForward::~BasicForward() {
        this->context.device.destroy_image(color_image);
        this->context.device.destroy_image(depth_image);
    }

    void BasicForward::render(daxa::CommandList& cmd_list, const std::shared_ptr<Scene>& scene, daxa::BufferDeviceAddress camera_buffer) {
        cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::COLOR_ATTACHMENT_OUTPUT_WRITE,
            .before_layout = daxa::ImageLayout::UNDEFINED,
            .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
            .image_id = this->color_image,
        });

        cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
            .before_layout = daxa::ImageLayout::UNDEFINED,
            .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
            .image_slice = {.image_aspect = daxa::ImageAspectFlagBits::DEPTH | daxa::ImageAspectFlagBits::STENCIL},
            .image_id = this->depth_image,
        });

        cmd_list.begin_renderpass({
            .color_attachments = {{
                .image_view = this->color_image.default_view(),
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = std::array<f32, 4>{0.2f, 0.4f, 1.0f, 1.0f},
            }},
            .depth_attachment = {{
                .image_view = this->depth_image.default_view(),
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = daxa::DepthValue{1.0f, 0},
            }},
            .render_area = {.x = 0, .y = 0, .width = static_cast<u32>(size.x), .height = static_cast<u32>(size.y)},
        });

        cmd_list.set_viewport({
            .x = 0.0f,
            .y = size.y,
            .width = size.x,
            .height = -size.y,
            .min_depth = 0.0f,
            .max_depth = 1.0f 
        });

        cmd_list.set_pipeline(draw_pipeline);

        scene->iterate([&](Entity entity){
            if(entity.has_component<ModelComponent>()) {
                auto& model = entity.get_component<ModelComponent>().model;

                DrawPush push_constant;
                push_constant.camera_info_buffer = camera_buffer;
                push_constant.object_info_buffer = entity.get_component<TransformComponent>().object_info->buffer_address;
                push_constant.lights_info_buffer = scene->lights_buffer->buffer_address;

                model->bind_index_buffer(cmd_list);
                model->draw(cmd_list, push_constant);
            }
        });

        cmd_list.end_renderpass();

        cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::READ,
            .before_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
            .after_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
            .image_id = this->color_image,
        });
    }

    void BasicForward::resize(u32 sx, u32 sy) {
        this->size = { static_cast<f32>(sx), static_cast<f32>(sy) };

        this->context.device.destroy_image(this->depth_image);
        this->depth_image = this->context.device.create_image({
            .format = daxa::Format::D24_UNORM_S8_UINT,
            .aspect = daxa::ImageAspectFlagBits::DEPTH | daxa::ImageAspectFlagBits::STENCIL,
            .size = { sx, sy, 1},
            .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT,
        });

        this->context.device.destroy_image(this->color_image);
        this->color_image = this->context.device.create_image({
            .dimensions = 2,
            .format = this->context.swapchain.get_format(),
            .aspect = daxa::ImageAspectFlagBits::COLOR,
            .size = { sx, sy, 1},
            .mip_level_count = 1,
            .array_layer_count = 1,
            .sample_count = 1,
            .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY
        });
    }

    void BasicForward::render_settings_ui() {
        ImGui::Begin("Basic Forward Settings");
        if(ImGui::TreeNodeEx("Texturing")) {
            if(ImGui::Checkbox("None", &this->settings.texturing.none)) {
                this->settings.texturing.vertex_color = false;
                this->settings.texturing.albedo = false;

                this->has_rebuild_pipeline = true;
            }

            if(ImGui::Checkbox("Vertex Color", &this->settings.texturing.vertex_color)) {
                this->settings.texturing.none = false;
                this->settings.texturing.albedo = false;

                this->has_rebuild_pipeline = true;
            }

            if(ImGui::Checkbox("Albedo", &this->settings.texturing.albedo)) {
                this->settings.texturing.none = false;
                this->settings.texturing.vertex_color = false;

                this->has_rebuild_pipeline = true;
            }

            ImGui::TreePop();
        }

        if(ImGui::TreeNodeEx("Shading Model")) {
            if(ImGui::Checkbox("None", &this->settings.shading_model.none)) {
                this->settings.shading_model.lambertian = false;
                this->settings.shading_model.phong = false;
                this->settings.shading_model.blinn_phong = false;
                this->settings.shading_model.gourad = false;
                this->settings.shading_model.gaussian = false;

                this->has_rebuild_pipeline = true;
            }

            if(ImGui::Checkbox("Lambertian", &this->settings.shading_model.lambertian)) {
                this->settings.shading_model.none = false;
                this->settings.shading_model.phong = false;
                this->settings.shading_model.blinn_phong = false;
                this->settings.shading_model.gourad = false;
                this->settings.shading_model.gaussian = false;

                this->has_rebuild_pipeline = true;
            }

            if(ImGui::Checkbox("Phong", &this->settings.shading_model.phong)) {
                this->settings.shading_model.none = false;
                this->settings.shading_model.lambertian = false;
                this->settings.shading_model.blinn_phong = false;
                this->settings.shading_model.gourad = false;
                this->settings.shading_model.gaussian = false;

                this->has_rebuild_pipeline = true;
            }

            if(ImGui::Checkbox("Blinn Phong", &this->settings.shading_model.blinn_phong)) {
                this->settings.shading_model.none = false;
                this->settings.shading_model.lambertian = false;
                this->settings.shading_model.phong = false;
                this->settings.shading_model.gourad = false;
                this->settings.shading_model.gaussian = false;

                this->has_rebuild_pipeline = true;
            }

            if(ImGui::Checkbox("Gourad", &this->settings.shading_model.gourad)) {
                this->settings.shading_model.none = false;
                this->settings.shading_model.lambertian = false;
                this->settings.shading_model.phong = false;
                this->settings.shading_model.blinn_phong = false;
                this->settings.shading_model.gaussian = false;

                this->has_rebuild_pipeline = true;
            }

            if(ImGui::Checkbox("Gaussian", &this->settings.shading_model.gaussian)) {
                this->settings.shading_model.none = false;
                this->settings.shading_model.lambertian = false;
                this->settings.shading_model.phong = false;
                this->settings.shading_model.blinn_phong = false;
                this->settings.shading_model.gourad = false;

                this->has_rebuild_pipeline = true;
            }

            ImGui::TreePop();
        }

        ImGui::End();
    }

    auto BasicForward::settings_to_string()-> std::string {
        std::string string = {};

        if(this->settings.texturing.none) {
            string += "#define SETTINGS_TEXTURING_NONE\n";
        }

        if(this->settings.texturing.vertex_color) {
            string += "#define SETTINGS_TEXTURING_VERTEX_COLOR\n";
        }

        if(this->settings.texturing.albedo) {
            string += "#define SETTINGS_TEXTURING_ALBEDO\n";
        }


        if(this->settings.shading_model.none) {
            string += "#define SETTINGS_SHADING_MODEL_NONE\n";
        }

        if(this->settings.shading_model.lambertian) {
            string += "#define SETTINGS_SHADING_MODEL_LAMBERTIAN\n";
        }

        if(this->settings.shading_model.phong) {
            string += "#define SETTINGS_SHADING_MODEL_PHONG\n";
        }

        if(this->settings.shading_model.blinn_phong) {
            string += "#define SETTINGS_SHADING_MODEL_BLINN_PHONG\n";
        }

        if(this->settings.shading_model.gourad) {
            string += "#define SETTINGS_SHADING_MODEL_GOURAD\n";
        }

        if(this->settings.shading_model.gaussian) {
            string += "#define SETTINGS_SHADING_MODEL_GAUSSIAN\n";
        }

        std::cout << string << std::endl;

        return std::move(string);
    }

    void BasicForward::rebuild_pipeline() {
        if(this->has_rebuild_pipeline) {
            this->draw_pipeline = this->context.pipeline_compiler.create_raster_pipeline({
                .vertex_shader_info = {
                    .source = daxa::ShaderCode{ this->settings_to_string() + file_to_string("./shaders/draw.glsl") }, 
                    .compile_options = { .defines = { daxa::ShaderDefine{"DRAW_VERT"} } }
                },
                .fragment_shader_info = {
                    .source = daxa::ShaderCode{ this->settings_to_string() + file_to_string("./shaders/draw.glsl") }, 
                    .compile_options = { .defines = { daxa::ShaderDefine{"DRAW_FRAG"} } }
                },
                .color_attachments = {{.format = this->context.swapchain.get_format(), .blend = {.blend_enable = true, .src_color_blend_factor = daxa::BlendFactor::SRC_ALPHA, .dst_color_blend_factor = daxa::BlendFactor::ONE_MINUS_SRC_ALPHA}}},
                .depth_test = {
                    .depth_attachment_format = daxa::Format::D24_UNORM_S8_UINT,
                    .enable_depth_test = true,
                    .enable_depth_write = true,
                },
                .raster = {
                    .polygon_mode = daxa::PolygonMode::FILL,
                    .face_culling = daxa::FaceCullFlagBits::FRONT_BIT,
                },
                .push_constant_size = sizeof(DrawPush),
                .debug_name = APPNAME_PREFIX("raster_pipeline"),
            }).value();

            this->has_rebuild_pipeline = false;
            std::cout << "pipeline reloaded" << std::endl;
        }
    }
}