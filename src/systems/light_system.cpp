#include "light_system.hpp"

namespace dare {
    LightSystem::LightSystem(std::shared_ptr<Scene> scene, daxa::Device& device, daxa::PipelineManager& pipeline_manager) : scene{scene}, device{device} {
        lights_buffer = std::make_unique<Buffer<LightsInfo>>(device);

        this->normal_shadow_pipeline = pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = {.source = daxa::ShaderFile{"normal_shadow.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_VERT"}}}},
            .fragment_shader_info = {.source = daxa::ShaderFile{"normal_shadow.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_FRAG"}}}},
            .color_attachments = {},
            .depth_test = {
                .depth_attachment_format = daxa::Format::D16_UNORM,
                .enable_depth_test = true,
                .enable_depth_write = true,
            },
            .raster = {
                .face_culling = daxa::FaceCullFlagBits::NONE,
            },
            .push_constant_size = sizeof(ShadowPush),
            .debug_name = "raster_pipeline",
        }).value();

        this->variance_shadow_pipeline = pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = {.source = daxa::ShaderFile{"variance_shadow.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_VERT"}}}},
            .fragment_shader_info = {.source = daxa::ShaderFile{"variance_shadow.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_FRAG"}}}},
            .color_attachments = {
                { .format = daxa::Format::R16G16_UNORM },
            },
            .depth_test = {
                .depth_attachment_format = daxa::Format::D16_UNORM,
                .enable_depth_test = true,
                .enable_depth_write = true,
            },
            .raster = {
                .face_culling = daxa::FaceCullFlagBits::NONE,
            },
            .push_constant_size = sizeof(ShadowPush),
            .debug_name = "raster_pipeline",
        }).value();

        this->filter_gauss_pipeline = pipeline_manager.add_raster_pipeline({
            .vertex_shader_info = {.source = daxa::ShaderFile{"filter_gauss.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_VERT"}}}},
            .fragment_shader_info = {.source = daxa::ShaderFile{"filter_gauss.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_FRAG"}}}},
            .color_attachments = {
                { .format = daxa::Format::R16G16_UNORM },
            },
            .raster = {
                .face_culling = daxa::FaceCullFlagBits::NONE,
            },
            .push_constant_size = sizeof(GaussPush),
            .debug_name = "raster_pipeline",
        }).value();

    }

    LightSystem::~LightSystem() {
        scene->iterate([&](Entity entity) {
            if(entity.has_component<DirectionalLightComponent>()) {
                auto& comp = entity.get_component<DirectionalLightComponent>();
                if(!comp.shadow_info.shadow_image.is_empty()) {
                    device.destroy_image(comp.shadow_info.shadow_image);
                }
            }

            if(entity.has_component<PointLightComponent>()) {
                auto& comp = entity.get_component<PointLightComponent>();
                if(!comp.shadow_info.shadow_image.is_empty()) {
                    device.destroy_image(comp.shadow_info.shadow_image);
                }
            }

            if(entity.has_component<SpotLightComponent>()) {
                auto& comp = entity.get_component<SpotLightComponent>();
                if(!comp.shadow_info.shadow_image.is_empty()) {
                    device.destroy_image(comp.shadow_info.shadow_image);
                }
                if(!comp.shadow_info.depth_image.is_empty()) {
                    device.destroy_image(comp.shadow_info.depth_image);
                }
                if(!comp.shadow_info.temp_shadow_image.is_empty()) {
                    device.destroy_image(comp.shadow_info.temp_shadow_image);
                }
            }
        });
    }

    void LightSystem::reload() {
        scene->iterate([&](Entity entity) {
            if(entity.has_component<DirectionalLightComponent>()) {
                auto& comp = entity.get_component<DirectionalLightComponent>();
                if(comp.shadow_info.has_to_create) {
                    comp.shadow_info.shadow_image = device.create_image({
                        .format = daxa::Format::D16_UNORM,
                        .aspect = daxa::ImageAspectFlagBits::DEPTH,
                        .size = {static_cast<u32>(comp.shadow_info.image_size.x), static_cast<u32>(comp.shadow_info.image_size.y), 1},
                        .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
                    });

                    comp.shadow_info.type = ShadowType::PCF;

                    std::cout << "Create shadow image for directional light" << std::endl;
                    comp.shadow_info.has_to_create = false;
                }

                if(comp.shadow_info.has_to_resize) {
                    device.destroy_image(comp.shadow_info.shadow_image);
                    comp.shadow_info.shadow_image = device.create_image({
                        .format = daxa::Format::D16_UNORM,
                        .aspect = daxa::ImageAspectFlagBits::DEPTH,
                        .size = {static_cast<u32>(comp.shadow_info.image_size.x), static_cast<u32>(comp.shadow_info.image_size.y), 1},
                        .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
                    });

                    std::cout << "Resized shadow image for directional light" << std::endl;
                    comp.shadow_info.has_to_resize = false;
                }

                if(comp.shadow_info.has_moved) {
                    f32 clip_space = comp.shadow_info.clip_space;
                    comp.shadow_info.projection = glm::ortho(-clip_space, clip_space, -clip_space, clip_space, -clip_space, clip_space);

                    glm::vec3 pos = entity.get_component<TransformComponent>().translation;
                    glm::vec3 dir = glm::normalize(entity.get_component<TransformComponent>().rotation);

                    glm::vec3 look_pos = pos + dir;
                    comp.shadow_info.view = glm::lookAt(pos, look_pos, glm::vec3(0.0, -1.0, 0.0));

                    comp.shadow_info.has_moved = false;
                    std::cout << "Moved shadow image for directional light" << std::endl;
                }
            }

            if(entity.has_component<PointLightComponent>()) {
                auto& comp = entity.get_component<PointLightComponent>();
            }

            if(entity.has_component<SpotLightComponent>()) {
                auto& comp = entity.get_component<SpotLightComponent>();
                if(comp.shadow_info.has_to_create) {
                    comp.shadow_info.depth_image = device.create_image({
                        .format = daxa::Format::D16_UNORM,
                        .aspect = daxa::ImageAspectFlagBits::DEPTH,
                        .size = {static_cast<u32>(comp.shadow_info.image_size.x), static_cast<u32>(comp.shadow_info.image_size.y), 1},
                        .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
                    });

                    comp.shadow_info.shadow_image = device.create_image({
                        .format = daxa::Format::R16G16_UNORM,
                        .aspect = daxa::ImageAspectFlagBits::COLOR,
                        .size = {static_cast<u32>(comp.shadow_info.image_size.x), static_cast<u32>(comp.shadow_info.image_size.y), 1},
                        .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
                    });

                    comp.shadow_info.temp_shadow_image = device.create_image({
                        .format = daxa::Format::R16G16_UNORM,
                        .aspect = daxa::ImageAspectFlagBits::COLOR,
                        .size = {static_cast<u32>(comp.shadow_info.image_size.x), static_cast<u32>(comp.shadow_info.image_size.y), 1},
                        .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
                    });

                    comp.shadow_info.type = ShadowType::VARIANCE;

                    std::cout << "Create shadow image for spot light" << std::endl;
                    comp.shadow_info.has_to_create = false;
                }

                if(comp.shadow_info.has_to_resize) {
                    device.destroy_image(comp.shadow_info.shadow_image);
                    device.destroy_image(comp.shadow_info.depth_image);
                    device.destroy_image(comp.shadow_info.temp_shadow_image);
                    comp.shadow_info.depth_image = device.create_image({
                        .format = daxa::Format::D16_UNORM,
                        .aspect = daxa::ImageAspectFlagBits::DEPTH,
                        .size = {static_cast<u32>(comp.shadow_info.image_size.x), static_cast<u32>(comp.shadow_info.image_size.y), 1},
                        .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
                    });

                    comp.shadow_info.shadow_image = device.create_image({
                        .format = daxa::Format::R16G16_UNORM,
                        .aspect = daxa::ImageAspectFlagBits::COLOR,
                        .size = {static_cast<u32>(comp.shadow_info.image_size.x), static_cast<u32>(comp.shadow_info.image_size.y), 1},
                        .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
                    });

                    comp.shadow_info.temp_shadow_image = device.create_image({
                        .format = daxa::Format::R16G16_UNORM,
                        .aspect = daxa::ImageAspectFlagBits::COLOR,
                        .size = {static_cast<u32>(comp.shadow_info.image_size.x), static_cast<u32>(comp.shadow_info.image_size.y), 1},
                        .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
                    });

                    std::cout << "Resized shadow image for spot light" << std::endl;
                    comp.shadow_info.has_to_resize = false;
                }

                if(comp.shadow_info.has_moved) {
                    f32 clip_space = comp.shadow_info.clip_space;
                    //comp.shadow_info.projection = glm::ortho(-clip_space, clip_space, -clip_space, clip_space, -clip_space, clip_space);

                    comp.shadow_info.projection = glm::perspective(glm::radians(comp.outer_cut_off), 1.0f, 0.1f, 100.0f);
                    comp.shadow_info.projection[1][1] *= -1.0f;

                    prj = comp.shadow_info.projection;

                    glm::vec3 pos = entity.get_component<TransformComponent>().translation;
                    glm::vec3 dir = glm::normalize(entity.get_component<TransformComponent>().rotation);

                    glm::vec3 look_pos = pos + dir;
                    comp.shadow_info.view = glm::lookAt(pos, look_pos, glm::vec3(0.0, 1.0, 0.0));

                    vie = comp.shadow_info.view;

                    comp.shadow_info.has_moved = false;
                    std::cout << "Moved shadow image for spot light" << std::endl;
                }
            }
        });
    }

    void LightSystem::update(daxa::CommandList& cmd_list, daxa::SamplerId& sampler) {
        LightsInfo info;
        info.num_directional_lights = 0;
        info.num_point_lights = 0;
        info.num_spot_lights = 0;

        scene->iterate([&](Entity entity) {
            if(entity.has_component<DirectionalLightComponent>()) {
                auto& comp = entity.get_component<DirectionalLightComponent>();
                glm::vec3 dir = glm::normalize(entity.get_component<TransformComponent>().rotation);
                glm::vec3 col = comp.color;
                info.directional_lights[info.num_directional_lights].direction = *reinterpret_cast<const f32vec3 *>(&dir);
                info.directional_lights[info.num_directional_lights].color = *reinterpret_cast<const f32vec3 *>(&col);
                info.directional_lights[info.num_directional_lights].intensity = comp.intensity;
                info.directional_lights[info.num_directional_lights].shadow_image = TextureId { .image_view_id = comp.shadow_info.shadow_image.default_view(), .sampler_id = sampler };
                info.directional_lights[info.num_directional_lights].shadow_type = static_cast<i32>(comp.shadow_info.type);

                glm::mat4 light_matrix = comp.shadow_info.projection * comp.shadow_info.view;

                info.directional_lights[info.num_directional_lights].light_matrix = *reinterpret_cast<const f32mat4x4*>(&light_matrix);

                render_normal_shadow_map(cmd_list, light_matrix, comp.shadow_info.image_size, comp.shadow_info.shadow_image);

                info.num_directional_lights++;
                return;
            }

            if(entity.has_component<PointLightComponent>()) {
                auto& comp = entity.get_component<PointLightComponent>();
                glm::vec3 pos = entity.get_component<TransformComponent>().translation;
                glm::vec3 col = comp.color;
                info.point_lights[info.num_point_lights].position = *reinterpret_cast<const f32vec3 *>(&pos);
                info.point_lights[info.num_point_lights].color = *reinterpret_cast<const f32vec3 *>(&col);
                info.point_lights[info.num_point_lights].intensity = comp.intensity;
                info.num_point_lights++;
                return;
            }

            if(entity.has_component<SpotLightComponent>()) {
                auto& comp = entity.get_component<SpotLightComponent>();
                glm::vec3 pos = entity.get_component<TransformComponent>().translation;
                glm::vec3 dir = glm::normalize(entity.get_component<TransformComponent>().rotation);
                glm::vec3 col = comp.color;
                info.spot_lights[info.num_spot_lights].position = *reinterpret_cast<const f32vec3 *>(&pos);
                info.spot_lights[info.num_spot_lights].direction = *reinterpret_cast<const f32vec3 *>(&dir);
                info.spot_lights[info.num_spot_lights].color = *reinterpret_cast<const f32vec3 *>(&col);
                info.spot_lights[info.num_spot_lights].intensity = comp.intensity;
                info.spot_lights[info.num_spot_lights].cut_off = glm::cos(glm::radians(comp.cut_off));
                info.spot_lights[info.num_spot_lights].outer_cut_off = glm::cos(glm::radians(comp.outer_cut_off));
                info.spot_lights[info.num_spot_lights].shadow_image = TextureId { .image_view_id = comp.shadow_info.shadow_image.default_view(), .sampler_id = sampler };
                info.spot_lights[info.num_spot_lights].shadow_type = static_cast<i32>(comp.shadow_info.type);

                glm::mat4 light_matrix = comp.shadow_info.projection * comp.shadow_info.view;

                info.spot_lights[info.num_spot_lights].light_matrix = *reinterpret_cast<const f32mat4x4*>(&light_matrix);

                render_variance_shadow_map(cmd_list, light_matrix, comp.shadow_info.image_size, comp.shadow_info.depth_image, comp.shadow_info.temp_shadow_image, comp.shadow_info.shadow_image);

                info.num_spot_lights++;
                return;
            }

        });

        lights_buffer->update(cmd_list, info);
    }

    void LightSystem::render_normal_shadow_map(daxa::CommandList& cmd_list, const glm::mat4& vp, glm::ivec2 size, daxa::ImageId depth_image) {
        u32 size_x = static_cast<u32>(size.x);
        u32 size_y = static_cast<u32>(size.y);
        
        cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
            .before_layout = daxa::ImageLayout::UNDEFINED,
            .after_layout = daxa::ImageLayout::GENERAL,
            .image_slice = {.image_aspect = daxa::ImageAspectFlagBits::DEPTH },
            .image_id = depth_image,
        });

        cmd_list.begin_renderpass({
            .depth_attachment = {{
                .image_view = depth_image.default_view(),
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = daxa::DepthValue{1.0f, 0},
            }},
            .render_area = {.x = 0, .y = 0, .width = size_x, .height = size_y},
        });
        cmd_list.set_pipeline(*normal_shadow_pipeline);

        ShadowPush push;
        push.light_matrix = *reinterpret_cast<const f32mat4x4*>(&vp);

        scene->iterate([&](Entity entity){
            if(entity.has_component<ModelComponent>()) {
                auto& model = entity.get_component<ModelComponent>().model;

                push.object_buffer = entity.get_component<TransformComponent>().object_info->buffer_address;
                push.face_buffer = model->vertex_buffer_address;

                model->bind_index_buffer(cmd_list);
                cmd_list.push_constant(push);
                model->draw(cmd_list);
            }
        });

        cmd_list.end_renderpass();
    }

    void LightSystem::render_variance_shadow_map(daxa::CommandList& cmd_list, const glm::mat4& vp, glm::ivec2 size, daxa::ImageId depth_image, daxa::ImageId temp_image, daxa::ImageId variance_image) {
        u32 size_x = static_cast<u32>(size.x);
        u32 size_y = static_cast<u32>(size.y);
        
        cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
            .before_layout = daxa::ImageLayout::UNDEFINED,
            .after_layout = daxa::ImageLayout::GENERAL,
            .image_slice = {.image_aspect = daxa::ImageAspectFlagBits::DEPTH },
            .image_id = depth_image,
        });

        cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
            .before_layout = daxa::ImageLayout::UNDEFINED,
            .after_layout = daxa::ImageLayout::GENERAL,
            .image_slice = {.image_aspect = daxa::ImageAspectFlagBits::COLOR },
            .image_id = variance_image,
        });

        cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
            .before_layout = daxa::ImageLayout::UNDEFINED,
            .after_layout = daxa::ImageLayout::GENERAL,
            .image_slice = {.image_aspect = daxa::ImageAspectFlagBits::COLOR },
            .image_id = temp_image,
        });

        cmd_list.begin_renderpass({
            .color_attachments = {
                {
                    .image_view = variance_image.default_view(),
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<f32, 4>{1.0f, 1.0f, 1.0f, 1.0f},
                },
            },
            .depth_attachment = {{
                .image_view = depth_image.default_view(),
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = daxa::DepthValue{1.0f, 0},
            }},
            .render_area = {.x = 0, .y = 0, .width = size_x, .height = size_y},
        });
        cmd_list.set_pipeline(*variance_shadow_pipeline);

        ShadowPush push;
        push.light_matrix = *reinterpret_cast<const f32mat4x4*>(&vp);

        scene->iterate([&](Entity entity){
            if(entity.has_component<ModelComponent>()) {
                auto& model = entity.get_component<ModelComponent>().model;

                push.object_buffer = entity.get_component<TransformComponent>().object_info->buffer_address;
                push.face_buffer = model->vertex_buffer_address;

                model->bind_index_buffer(cmd_list);
                cmd_list.push_constant(push);
                model->draw(cmd_list);
            }
        });

        cmd_list.end_renderpass();

        cmd_list.begin_renderpass({
            .color_attachments = {
                {
                    .image_view = temp_image.default_view(),
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<f32, 4>{1.0f, 1.0f, 1.0f, 1.0f},
                },
            },
            .render_area = {.x = 0, .y = 0, .width = size_x, .height = size_y},
        });
        cmd_list.set_pipeline(*filter_gauss_pipeline);

        
        cmd_list.push_constant(GaussPush {
            .src_texture = variance_image.default_view(),
            .blur_scale = { 1.0f / static_cast<f32>(size_x), 0.0f }
        });
        cmd_list.draw({ .vertex_count = 3});

        cmd_list.end_renderpass();

        cmd_list.begin_renderpass({
            .color_attachments = {
                {
                    .image_view = variance_image.default_view(),
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<f32, 4>{1.0f, 1.0f, 1.0f, 1.0f},
                },
            },
            .render_area = {.x = 0, .y = 0, .width = size_x, .height = size_y},
        });
        cmd_list.set_pipeline(*filter_gauss_pipeline);

        
        cmd_list.push_constant(GaussPush {
            .src_texture = temp_image.default_view(),
            .blur_scale = { 0.0f, 1.0f / static_cast<f32>(size_y) }
        });
        cmd_list.draw({ .vertex_count = 3});

        cmd_list.end_renderpass();
    }
}