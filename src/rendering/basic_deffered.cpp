#include "basic_deffered.hpp"

#include <imgui.h>

namespace dare {
    BasicDeffered::BasicDeffered(RenderContext& context) : Task(context) {
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

        this->albedo_image = this->context.device.create_image({
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

        this->normal_image = this->context.device.create_image({
            .dimensions = 2,
            .format = daxa::Format::R16G16B16A16_SFLOAT,
            .aspect = daxa::ImageAspectFlagBits::COLOR,
            .size = { sx, sy, 1 },
            .mip_level_count = 1,
            .array_layer_count = 1,
            .sample_count = 1,
            .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY
        });

        this->position_image = this->context.device.create_image({
            .dimensions = 2,
            .format = daxa::Format::R16G16B16A16_SFLOAT,
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

        this->sampler = this->context.device.create_sampler({
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
            .max_lod = static_cast<f32>(1),
            .enable_unnormalized_coordinates = false,
        });

        rebuild_pipeline();
    }

    BasicDeffered::~BasicDeffered() {
        this->context.device.destroy_image(color_image);
        this->context.device.destroy_image(albedo_image);
        this->context.device.destroy_image(normal_image);
        this->context.device.destroy_image(position_image);
        this->context.device.destroy_image(depth_image);
        this->context.device.destroy_sampler(sampler);
    }

    void BasicDeffered::render(daxa::CommandList& cmd_list, const std::shared_ptr<Scene>& scene, daxa::BufferDeviceAddress camera_buffer) {
        cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::COLOR_ATTACHMENT_OUTPUT_WRITE,
            .before_layout = daxa::ImageLayout::UNDEFINED,
            .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
            .image_id = this->color_image,
        });

        cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::COLOR_ATTACHMENT_OUTPUT_WRITE,
            .before_layout = daxa::ImageLayout::UNDEFINED,
            .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
            .image_id = this->albedo_image,
        });

        cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::COLOR_ATTACHMENT_OUTPUT_WRITE,
            .before_layout = daxa::ImageLayout::UNDEFINED,
            .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
            .image_id = this->normal_image,
        });

        cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::COLOR_ATTACHMENT_OUTPUT_WRITE,
            .before_layout = daxa::ImageLayout::UNDEFINED,
            .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
            .image_id = this->position_image,
        });

        cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
            .before_layout = daxa::ImageLayout::UNDEFINED,
            .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
            .image_slice = {.image_aspect = daxa::ImageAspectFlagBits::DEPTH | daxa::ImageAspectFlagBits::STENCIL},
            .image_id = this->depth_image,
        });

        cmd_list.begin_renderpass({
            .color_attachments = {
                {
                    .image_view = this->albedo_image.default_view(),
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<f32, 4>{0.0f, 0.0f, 0.0f, 1.0f},
                },
                {
                    .image_view = this->normal_image.default_view(),
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<f32, 4>{0.0f, 0.0f, 0.0f, 1.0f},
                },
                {
                    .image_view = this->position_image.default_view(),
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<f32, 4>{0.0f, 0.0f, 0.0f, 1.0f},
                },
            },
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

        cmd_list.set_pipeline(g_buffer_gather_pipeline);

        scene->iterate([&](Entity entity){
            if(entity.has_component<ModelComponent>()) {
                auto& model = entity.get_component<ModelComponent>().model;

                DrawPush push_constant;
                push_constant.camera_buffer = camera_buffer;
                push_constant.object_buffer = entity.get_component<TransformComponent>().object_info->buffer_address;
                push_constant.lights_buffer = scene->lights_buffer->buffer_address;

                model->bind_index_buffer(cmd_list);
                model->draw(cmd_list, push_constant);
            }
        });

        cmd_list.end_renderpass();

        cmd_list.begin_renderpass({
            .color_attachments = {
                {
                    .image_view = this->color_image.default_view(),
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<f32, 4>{0.0f, 0.0f, 0.0f, 1.0f},
                },
            },
            .depth_attachment = {{
                .image_view = this->depth_image.default_view(),
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = daxa::DepthValue{1.0f, 0},
            }},
            .render_area = {.x = 0, .y = 0, .width = static_cast<u32>(size.x), .height = static_cast<u32>(size.y)},
        });

        cmd_list.set_pipeline(composition_pipeline);

        cmd_list.push_constant(CompositionPush {
            .albedo = { .image_view_id = albedo_image.default_view(), .sampler_id = sampler },
            .normal = { .image_view_id = normal_image.default_view(), .sampler_id = sampler },
            .position = { .image_view_id = position_image.default_view(), .sampler_id = sampler },
            .camera_buffer = camera_buffer,
            .lights_buffer = scene->lights_buffer->buffer_address
        });
        cmd_list.draw({ .vertex_count = 3 });

        cmd_list.end_renderpass();

        cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::READ,
            .before_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
            .after_layout = daxa::ImageLayout::READ_ONLY_OPTIMAL,
            .image_id = this->color_image,
        });
    }

    void BasicDeffered::resize(u32 sx, u32 sy) {
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

        this->context.device.destroy_image(this->albedo_image);
        this->albedo_image = this->context.device.create_image({
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

        this->context.device.destroy_image(this->normal_image);
        this->normal_image = this->context.device.create_image({
            .dimensions = 2,
            .format = daxa::Format::R16G16B16A16_SFLOAT,
            .aspect = daxa::ImageAspectFlagBits::COLOR,
            .size = { sx, sy, 1},
            .mip_level_count = 1,
            .array_layer_count = 1,
            .sample_count = 1,
            .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY
        });

        this->context.device.destroy_image(this->position_image);
        this->position_image = this->context.device.create_image({
            .dimensions = 2,
            .format = daxa::Format::R16G16B16A16_SFLOAT,
            .aspect = daxa::ImageAspectFlagBits::COLOR,
            .size = { sx, sy, 1},
            .mip_level_count = 1,
            .array_layer_count = 1,
            .sample_count = 1,
            .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY
        });
    }

    void BasicDeffered::render_settings_ui() {
        ImGui::Begin("Basic Deffered Settings");
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
                this->settings.shading_model.gaussian = false;

                this->has_rebuild_pipeline = true;
            }

            if(ImGui::Checkbox("Lambertian", &this->settings.shading_model.lambertian)) {
                this->settings.shading_model.none = false;
                this->settings.shading_model.phong = false;
                this->settings.shading_model.blinn_phong = false;
                this->settings.shading_model.gaussian = false;

                this->has_rebuild_pipeline = true;
            }

            if(ImGui::Checkbox("Phong", &this->settings.shading_model.phong)) {
                this->settings.shading_model.none = false;
                this->settings.shading_model.lambertian = false;
                this->settings.shading_model.blinn_phong = false;
                this->settings.shading_model.gaussian = false;

                this->has_rebuild_pipeline = true;
            }

            if(ImGui::Checkbox("Blinn Phong", &this->settings.shading_model.blinn_phong)) {
                this->settings.shading_model.none = false;
                this->settings.shading_model.lambertian = false;
                this->settings.shading_model.phong = false;
                this->settings.shading_model.gaussian = false;

                this->has_rebuild_pipeline = true;
            }

            if(ImGui::Checkbox("Gaussian", &this->settings.shading_model.gaussian)) {
                this->settings.shading_model.none = false;
                this->settings.shading_model.lambertian = false;
                this->settings.shading_model.phong = false;
                this->settings.shading_model.blinn_phong = false;

                this->has_rebuild_pipeline = true;
            }

            ImGui::TreePop();
        }

        if(ImGui::TreeNodeEx("Normal mapping")) {
            if(ImGui::Checkbox("None", &this->settings.normal_mappings.none)) {
                this->settings.normal_mappings.using_tangents = false;
                this->settings.normal_mappings.calculating_TBN_vectors = false;

                this->has_rebuild_pipeline = true;
            }

            if(ImGui::Checkbox("Using Tangents", &this->settings.normal_mappings.using_tangents)) {
                this->settings.normal_mappings.none = false;
                this->settings.normal_mappings.calculating_TBN_vectors = false;

                this->has_rebuild_pipeline = true;
            }

            if(ImGui::Checkbox("Calculating Tangents", &this->settings.normal_mappings.calculating_TBN_vectors)) {
                this->settings.normal_mappings.none = false;
                this->settings.normal_mappings.using_tangents = false;

                this->has_rebuild_pipeline = true;
            }

            if(ImGui::Checkbox("Re-orthogonalize TBN", &this->settings.normal_mappings.reorthogonalize_TBN_vectors)) {
                this->has_rebuild_pipeline = true;
            }

            ImGui::TreePop();
        }

        if(ImGui::TreeNodeEx("Visualize Attachments")) {
            if(ImGui::Checkbox("None", &this->settings.visualize_attachments.none)) {
                this->settings.visualize_attachments.albedo = false;
                this->settings.visualize_attachments.normal = false;
                this->settings.visualize_attachments.position = false;

                this->has_rebuild_pipeline = true;
            }

            if(ImGui::Checkbox("Albedo", &this->settings.visualize_attachments.albedo)) {
                this->settings.visualize_attachments.none = false;
                this->settings.visualize_attachments.normal = false;
                this->settings.visualize_attachments.position = false;

                this->has_rebuild_pipeline = true;
            }

            if(ImGui::Checkbox("Normal", &this->settings.visualize_attachments.normal)) {
                this->settings.visualize_attachments.none = false;
                this->settings.visualize_attachments.albedo = false;
                this->settings.visualize_attachments.position = false;

                this->has_rebuild_pipeline = true;
            }

            if(ImGui::Checkbox("Position", &this->settings.visualize_attachments.position)) {
                this->settings.visualize_attachments.none = false;
                this->settings.visualize_attachments.albedo = false;
                this->settings.visualize_attachments.normal = false;

                this->has_rebuild_pipeline = true;
            }

            ImGui::TreePop();
        }

        ImGui::End();
    }

    auto BasicDeffered::settings_to_string()-> std::string {
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

        if(this->settings.shading_model.gaussian) {
            string += "#define SETTINGS_SHADING_MODEL_GAUSSIAN\n";
        }

        if(this->settings.normal_mappings.none) {
            string += "#define SETTINGS_NORMAL_MAPPING_NONE\n";
        }

        if(this->settings.normal_mappings.using_tangents) {
            string += "#define SETTINGS_NORMAL_MAPPING_USING_TANGENTS\n";
        }

        if(this->settings.normal_mappings.calculating_TBN_vectors) {
            string += "#define SETTINGS_NORMAL_MAPPING_CALCULATING_TBN_VECTORS\n";
        }

        if(this->settings.normal_mappings.reorthogonalize_TBN_vectors) {
            string += "#define SETTINGS_NORMAL_MAPPING_REORTHOGONALIZE_TBN_VECTORS\n";
        }

        if(this->settings.visualize_attachments.none) {
            string += "#define SETTINGS_VISUALIZE_ATTACHMENT_NONE\n";
        }

        if(this->settings.visualize_attachments.albedo) {
            string += "#define SETTINGS_VISUALIZE_ATTACHMENT_ALBEDO\n";
        }

        if(this->settings.visualize_attachments.normal) {
            string += "#define SETTINGS_VISUALIZE_ATTACHMENT_NORMAL\n";
        }

        if(this->settings.visualize_attachments.position) {
            string += "#define SETTINGS_VISUALIZE_ATTACHMENT_POSITION\n";
        }

        return std::move(string);
    }

    void BasicDeffered::rebuild_pipeline() {
        if(this->has_rebuild_pipeline) {
            std::string g_buffer_gather_code = this->settings_to_string() + file_to_string("./shaders/basic_deffered/g_buffer_gather.glsl");
            this->g_buffer_gather_pipeline = this->context.pipeline_compiler.create_raster_pipeline({
                .vertex_shader_info = {
                    .source = daxa::ShaderCode{ g_buffer_gather_code }, 
                    .compile_options = { .defines = { daxa::ShaderDefine{"DRAW_VERT"} } }
                },
                .fragment_shader_info = {
                    .source = daxa::ShaderCode{ g_buffer_gather_code }, 
                    .compile_options = { .defines = { daxa::ShaderDefine{"DRAW_FRAG"} } }
                },
                .color_attachments = {
                    { .format = this->context.swapchain.get_format(), },
                    { .format = daxa::Format::R16G16B16A16_SFLOAT },
                    { .format = daxa::Format::R16G16B16A16_SFLOAT, }
                },
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
                .debug_name = APPNAME_PREFIX("g_buffer_gather_pipeline"),
            }).value();
            std::string composition_code = this->settings_to_string() + file_to_string("./shaders/basic_deffered/composition.glsl");
            this->composition_pipeline = this->context.pipeline_compiler.create_raster_pipeline({
                .vertex_shader_info = {
                    .source = daxa::ShaderCode{ composition_code }, 
                    .compile_options = { .defines = { daxa::ShaderDefine{"DRAW_VERT"} } }
                },
                .fragment_shader_info = {
                    .source = daxa::ShaderCode{ composition_code }, 
                    .compile_options = { .defines = { daxa::ShaderDefine{"DRAW_FRAG"} } }
                },
                .color_attachments = {
                    {
                        .format = this->context.swapchain.get_format(), 
                        .blend = {
                            .blend_enable = true, 
                            .src_color_blend_factor = daxa::BlendFactor::SRC_ALPHA, 
                            .dst_color_blend_factor = daxa::BlendFactor::ONE_MINUS_SRC_ALPHA
                        }
                    }
                },
                .depth_test = {
                    .depth_attachment_format = daxa::Format::D24_UNORM_S8_UINT,
                    .enable_depth_test = true,
                    .enable_depth_write = true,
                },
                .raster = {
                    .polygon_mode = daxa::PolygonMode::FILL,
                },
                .push_constant_size = sizeof(DrawPush),
                .debug_name = APPNAME_PREFIX("composition_pipeline"),
            }).value();

            this->has_rebuild_pipeline = false;
            std::cout << "pipeline reloaded" << std::endl;
        }
    }
}