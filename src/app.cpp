#include "app.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include "data/entity.hpp"
#include "data/components.hpp"
#include "data/scene_serializer.hpp"

#include <thread>

App::App() : AppWindow<App>("poggers")  {
    this->context = daxa::create_context({
        .enable_validation = true
    });

    this->device = context.create_device({
        .debug_name = "my gpu"
    });

    this->swapchain = device.create_swapchain({
        .native_window = get_native_handle(glfw_window_ptr),
        .present_mode = daxa::PresentMode::DO_NOT_WAIT_FOR_VBLANK,
        .image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST,
        .debug_name = "swapchain"
    });

    this->pipeline_manager = daxa::PipelineManager({
        .device = device,
        .shader_compile_options = {
            .root_paths = {
                "/shaders",
                "../shaders",
                "../../shaders",
                "../../../shaders",
                "shaders",
                DAXA_SHADER_INCLUDE_DIR,
            },
            .language = daxa::ShaderLanguage::GLSL,
            .enable_debug_info = true,
        },
        .debug_name = "pipeline_manager",
    });

    this->deffered_pipeline = pipeline_manager.add_raster_pipeline({
        .vertex_shader_info = {.source = daxa::ShaderFile{"deffered.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_VERT"}}}},
        .fragment_shader_info = {.source = daxa::ShaderFile{"deffered.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_FRAG"}}}},
        .color_attachments = {
            { .format = swapchain.get_format() },
            { .format = daxa::Format::R16G16B16A16_SFLOAT },
            { .format = daxa::Format::R16G16B16A16_SFLOAT },
        },
        .depth_test = {
            .depth_attachment_format = daxa::Format::D32_SFLOAT,
            .enable_depth_test = true,
            .enable_depth_write = true,
        },
        .raster = {
            .face_culling = daxa::FaceCullFlagBits::NONE
        },
        .push_constant_size = sizeof(CompositionPush),
        .debug_name = "raster_pipeline",
    }).value();

    this->composition_pipeline = pipeline_manager.add_raster_pipeline({
        .vertex_shader_info = {.source = daxa::ShaderFile{"composition.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_VERT"}}}},
        .fragment_shader_info = {.source = daxa::ShaderFile{"composition.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_FRAG"}}}},
        .color_attachments = {
            { .format = swapchain.get_format() },
        },
        .raster = {
            .face_culling = daxa::FaceCullFlagBits::NONE
        },
        .push_constant_size = sizeof(CompositionPush),
        .debug_name = "raster_pipeline",
    }).value();

    this->depth_image = device.create_image({
        .format = daxa::Format::D32_SFLOAT,
        .aspect = daxa::ImageAspectFlagBits::DEPTH,
        .size = {size_x, size_y, 1},
        .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
    });

    this->albedo_image = device.create_image({
        .format = this->swapchain.get_format(),
        .aspect = daxa::ImageAspectFlagBits::COLOR,
        .size = {size_x, size_y, 1},
        .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
    });

    this->normal_image = device.create_image({
        .format = daxa::Format::R16G16B16A16_SFLOAT,
        .aspect = daxa::ImageAspectFlagBits::COLOR,
        .size = {size_x, size_y, 1},
        .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
    });

    this->emissive_image = device.create_image({
        .format = daxa::Format::R16G16B16A16_SFLOAT,
        .aspect = daxa::ImageAspectFlagBits::COLOR,
        .size = {size_x, size_y, 1},
        .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
    });

    this->ssao_image = this->device.create_image({
        .format = daxa::Format::R8_UNORM,
        .aspect = daxa::ImageAspectFlagBits::COLOR,
        .size = { half_size_x, half_size_y, 1 },
        .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
    });

    this->ssao_blur_image = this->device.create_image({
        .format = daxa::Format::R8_UNORM,
        .aspect = daxa::ImageAspectFlagBits::COLOR,
        .size = { half_size_x, half_size_y, 1 },
        .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
    });

    this->sampler = this->device.create_sampler({
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
            {
                .format = daxa::Format::R8_UNORM, 
            }
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
            {
                .format = daxa::Format::R8_UNORM, 
            }
        },
        .raster = {
            .polygon_mode = daxa::PolygonMode::FILL,
        },
        .push_constant_size = sizeof(SSAOBlurPush),
        .debug_name = "ssao_blur_pipeline",
    }).value();

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(glfw_window_ptr, true);
    imgui_renderer =  daxa::ImGuiRenderer({
        .device = device,
        .format = swapchain.get_format(),
    });

    bloom_renderer = std::make_unique<BloomRenderer>(pipeline_manager, device, glm::ivec2{size_x, size_y});

    this->scene = SceneSerializer::deserialize(this->device, "test.scene");
    //this->scene = std::make_shared<Scene>(device);
    this->scene_hiearchy = std::make_shared<SceneHiearchyPanel>(this->scene);
    camera_buffer = std::make_unique<Buffer<CameraInfo>>(this->device);

    camera.camera.resize(size_x, size_y);

    Entity entity = scene->create_entity("helment");
    auto model = std::make_shared<Model>(device, "assets/models/DamagedHelmet/glTF/DamagedHelmet.gltf");
    entity.add_component<ModelComponent>(model);

    auto& comp = entity.get_component<TransformComponent>();
    comp.translation = {0.0f, 3.0f, 0.0f};
    comp.rotation = {0.0f, 0.0f, 0.0f};
    comp.scale = {1.0f, 1.0f, 1.0f};

}

App::~App() {
    device.wait_idle();
    device.collect_garbage();
    device.destroy_image(depth_image);
    device.destroy_image(albedo_image);
    device.destroy_image(normal_image);
    device.destroy_image(emissive_image);
    device.destroy_image(ssao_image);
    device.destroy_image(ssao_blur_image);
    device.destroy_sampler(sampler);
    ImGui_ImplGlfw_Shutdown();
}

bool App::update() {
    glfwPollEvents();
    if (glfwWindowShouldClose(glfw_window_ptr)) {
        return true;
    }

    if (!minimized) {
        on_update();
    } else {
        using namespace std::literals;
        std::this_thread::sleep_for(1ms);
    }

    return false;
}

void App::ui_update() {
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Debug
    {
        ImGui::Begin("Debug");
        ImGui::Text("Frame time: %f", elapsed_s);
        ImGui::Text("Frame Per Second: %f", 1.0f / elapsed_s);
        ImGui::End();
    }

    scene_hiearchy->draw();

    ImGui::Render();
}

void App::on_update() {
    auto now = Clock::now();
    elapsed_s = std::chrono::duration<f32>(now - prev_time).count();
    prev_time = now;

    ui_update();

    camera.camera.set_pos(camera.pos);
    camera.camera.set_rot(camera.rot.x, camera.rot.y);
    camera.update(elapsed_s);

    render();
}

void App::render() {
    daxa::ImageId swapchain_image = swapchain.acquire_next_image();
    if(swapchain_image.is_empty()) {
        return;
    }

    scene->update();

    daxa::CommandList cmd_list = device.create_command_list({ .debug_name = "main loop cmd list" });

    {
        glm::mat4 view = camera.camera.get_view();

        glm::mat4 temp_inverse_projection_mat = glm::inverse(camera.camera.proj_mat);
        glm::mat4 temp_inverse_view_mat = glm::inverse(view);

        CameraInfo camera_info {
            .projection_matrix = *reinterpret_cast<const f32mat4x4*>(&camera.camera.proj_mat),
            .inverse_projection_matrix = *reinterpret_cast<const f32mat4x4*>(&temp_inverse_projection_mat),
            .view_matrix = *reinterpret_cast<const f32mat4x4*>(&view),
            .inverse_view_matrix = *reinterpret_cast<const f32mat4x4*>(&temp_inverse_view_mat),
            .position = *reinterpret_cast<const f32vec3*>(&camera.pos),
            .near_plane = camera.camera.near_clip,
            .far_plane = camera.camera.far_clip
        };

        camera_buffer->update(cmd_list, camera_info);
    }

    cmd_list.pipeline_barrier_image_transition({
        .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
        .before_layout = daxa::ImageLayout::UNDEFINED,
        .after_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
        .image_id = swapchain_image
    });

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
        .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
        .image_id = albedo_image
    });

    cmd_list.pipeline_barrier_image_transition({
        .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
        .before_layout = daxa::ImageLayout::UNDEFINED,
        .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
        .image_id = normal_image
    });

    cmd_list.pipeline_barrier_image_transition({
        .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
        .before_layout = daxa::ImageLayout::UNDEFINED,
        .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
        .image_id = emissive_image
    });

    cmd_list.begin_renderpass({
        .color_attachments = {
            {
                .image_view = this->albedo_image.default_view(),
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = std::array<f32, 4>{0.3f, 0.2f, 0.8f, 1.0f},
            },
            {
                .image_view = this->normal_image.default_view(),
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = std::array<f32, 4>{0.0f, 0.0f, 0.0f, 1.0f},
            },
            {
                .image_view = this->emissive_image.default_view(),
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = std::array<f32, 4>{0.0f, 0.0f, 0.0f, 1.0f},
            },
        },
        .depth_attachment = {{
            .image_view = depth_image.default_view(),
            .load_op = daxa::AttachmentLoadOp::CLEAR,
            .clear_value = daxa::DepthValue{1.0f, 0},
        }},
        .render_area = {.x = 0, .y = 0, .width = size_x, .height = size_y},
    });
    cmd_list.set_pipeline(*deffered_pipeline);

    DrawPush push;
    push.lights_buffer = scene->lights_buffer->buffer_address;
    push.camera_buffer = camera_buffer->buffer_address;

    scene->iterate([&](Entity entity){
        if(entity.has_component<ModelComponent>()) {
            auto& model = entity.get_component<ModelComponent>().model;

            push.object_buffer = entity.get_component<TransformComponent>().object_info->buffer_address;

            model->bind_index_buffer(cmd_list);
            model->draw(cmd_list, push);
        }
    });

    cmd_list.end_renderpass();

    // SSAO generation
    cmd_list.begin_renderpass({
        .color_attachments = {
            {
                .image_view = this->ssao_image.default_view(),
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = std::array<f32, 4>{1.0f, 0.0f, 0.0f, 1.0f},
            },
        },
        .render_area = {.x = 0, .y = 0, .width = static_cast<u32>(half_size_x), .height = static_cast<u32>(half_size_y)},
    });
    cmd_list.set_pipeline(*ssao_generation_pipeline);
    cmd_list.push_constant(SSAOGenerationPush {
        .normal = { .image_view_id = normal_image.default_view(), .sampler_id = sampler },
        .depth = { .image_view_id = depth_image.default_view(), .sampler_id = sampler },
        .camera_buffer = camera_buffer->buffer_address
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
        .render_area = {.x = 0, .y = 0, .width = static_cast<u32>(half_size_x), .height = static_cast<u32>(half_size_y)},
    });
    cmd_list.set_pipeline(*ssao_blur_pipeline);
    cmd_list.push_constant(SSAOBlurPush {
        .ssao = { .image_view_id = ssao_image.default_view(), .sampler_id = sampler },
    });
    cmd_list.draw({ .vertex_count = 3 });
    cmd_list.end_renderpass();

    bloom_renderer->render(cmd_list, emissive_image, sampler);

    cmd_list.begin_renderpass({
        .color_attachments = {
            {
                .image_view = swapchain_image.default_view(),
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = std::array<f32, 4>{0.0f, 0.0f, 0.0f, 1.0f},
            },
        },
        .render_area = {.x = 0, .y = 0, .width = static_cast<u32>(size_x), .height = static_cast<u32>(size_y)},
    });

    cmd_list.set_pipeline(*composition_pipeline);

    cmd_list.push_constant(CompositionPush {
        .albedo = { .image_view_id = albedo_image.default_view(), .sampler_id = sampler },
        .normal = { .image_view_id = normal_image.default_view(), .sampler_id = sampler },
        .emissive = { .image_view_id = emissive_image.default_view(), .sampler_id = sampler },
        .depth = { .image_view_id = depth_image.default_view(), .sampler_id = sampler },
        .ssao = { .image_view_id = ssao_blur_image.default_view(), .sampler_id = sampler },
        .lights_buffer = scene->lights_buffer->buffer_address,
        .camera_buffer = camera_buffer->buffer_address,
    });
    cmd_list.draw({ .vertex_count = 3 });

    cmd_list.end_renderpass();

    imgui_renderer.record_commands(ImGui::GetDrawData(), cmd_list, swapchain_image, size_x, size_y);

    cmd_list.pipeline_barrier_image_transition({
        .awaited_pipeline_access = daxa::AccessConsts::ALL_GRAPHICS_READ_WRITE,
        .before_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
        .after_layout = daxa::ImageLayout::PRESENT_SRC,
        .image_id = swapchain_image
    });

    cmd_list.complete();

    device.submit_commands({
        .command_lists = {std::move(cmd_list)},
        .wait_binary_semaphores = {swapchain.get_acquire_semaphore()},
        .signal_binary_semaphores = {swapchain.get_present_semaphore()},
        .signal_timeline_semaphores = {{swapchain.get_gpu_timeline_semaphore(), swapchain.get_cpu_timeline_value()}},
    });

    device.present_frame({
        .wait_binary_semaphores = {swapchain.get_present_semaphore()},
        .swapchain = swapchain,
    });
}

void App::on_mouse_scroll(f32 x, f32 y) {}
void App::on_mouse_button(int key, int action) {}
void App::on_mouse_move(f32 x, f32 y) {
    if (!paused) {
        f32 center_x = static_cast<f32>(size_x / 2);
        f32 center_y = static_cast<f32>(size_y / 2);
        auto offset = f32vec2{x - center_x, center_y - y};
        camera.on_mouse_move(offset.x, offset.y);
        glfwSetCursorPos(glfw_window_ptr, static_cast<f64>(center_x), static_cast<f64>(center_y));
    }
}

void App::on_key(i32 key, i32 action) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        toggle_pause();
    }

    if (!paused) {
        camera.on_key(key, action);
    }
}

void App::toggle_pause() {
    glfwSetCursorPos(glfw_window_ptr, static_cast<f64>(size_x / 2), static_cast<f64>(size_y / 2));
    glfwSetInputMode(glfw_window_ptr, GLFW_CURSOR, paused ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    glfwSetInputMode(glfw_window_ptr, GLFW_RAW_MOUSE_MOTION, paused);
    paused = !paused;
}

void App::on_resize(u32 sx, u32 sy) {
    minimized = (sx == 0 || sy == 0);
    if (!minimized) {
        swapchain.resize();
        size_x = swapchain.get_surface_extent().x;
        size_y = swapchain.get_surface_extent().y;
        half_size_x = size_x / 2;
        half_size_y = size_y / 2;
        camera.camera.resize(size_x, size_y);
        device.destroy_image(depth_image);
        depth_image = device.create_image({
            .format = daxa::Format::D32_SFLOAT,
            .aspect = daxa::ImageAspectFlagBits::DEPTH,
            .size = {size_x, size_y, 1},
            .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
        });
        device.destroy_image(albedo_image);
        this->albedo_image = device.create_image({
            .format = this->swapchain.get_format(),
            .aspect = daxa::ImageAspectFlagBits::COLOR,
            .size = {size_x, size_y, 1},
            .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
        });
        device.destroy_image(normal_image);
        this->normal_image = device.create_image({
            .format = daxa::Format::R16G16B16A16_SFLOAT,
            .aspect = daxa::ImageAspectFlagBits::COLOR,
            .size = {size_x, size_y, 1},
            .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
        });
        device.destroy_image(emissive_image);
        this->emissive_image = device.create_image({
            .format = daxa::Format::R16G16B16A16_SFLOAT,
            .aspect = daxa::ImageAspectFlagBits::COLOR,
            .size = {size_x, size_y, 1},
            .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
        });

        device.destroy_image(ssao_image);
        this->ssao_image = this->device.create_image({
            .format = daxa::Format::R8_UNORM,
            .aspect = daxa::ImageAspectFlagBits::COLOR,
            .size = { half_size_x, half_size_y, 1 },
            .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
        });

        device.destroy_image(ssao_blur_image);
        this->ssao_blur_image = this->device.create_image({
            .format = daxa::Format::R8_UNORM,
            .aspect = daxa::ImageAspectFlagBits::COLOR,
            .size = { half_size_x, half_size_y, 1 },
            .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
        });

        bloom_renderer->resize({size_x, size_y});

        on_update();
    }
}