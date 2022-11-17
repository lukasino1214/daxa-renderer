#include "rendering_system.hpp"

#include "../data/entity.hpp"
#include "../data/components.hpp"

namespace dare {
    RenderingSystem::RenderingSystem(std::unique_ptr<Window>& window) : window{window} {
        // setup context
        this->context.context = daxa::create_context({
            .enable_validation = false,
        });

        this->context.device = this->context.context.create_device({
            .debug_name = "device"
        });

        this->context.swapchain = this->context.device.create_swapchain({
            .native_window = window->get_native_handle(),
            .present_mode = daxa::PresentMode::DO_NOT_WAIT_FOR_VBLANK,
            .image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST,
            .debug_name = "swapchain",
        });

        this->context.pipeline_compiler = this->context.device.create_pipeline_compiler({
            .shader_compile_options = {
                .root_paths = {
                    ".out/release/vcpkg_installed/x64-linux/include",
                    "build/vcpkg_installed/x64-linux/include",
                    "vcpkg_installed/x64-linux/include",
                    "shaders",
                    "../shaders",
                    "include",
                },
                .language = daxa::ShaderLanguage::GLSL,
            },
            .debug_name = "pipeline_compiler",
        });

        // setup imgui
        ImGui::CreateContext();
        imgui_renderer = daxa::ImGuiRenderer({
            .device = this->context.device,
            .pipeline_compiler = this->context.pipeline_compiler,
            .format = this->context.swapchain.get_format(),
        });

        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        this->ibl_renderer = std::make_unique<IBLRenderer>(this->context.device, this->context.pipeline_compiler, this->context.swapchain.get_format());
    
        this->camera_buffer = std::make_unique<Buffer<CameraInfo>>(this->context.device);

        this->depth_image = this->context.device.create_image({
            .dimensions = 2,
            .format = daxa::Format::D24_UNORM_S8_UINT,
            .aspect = daxa::ImageAspectFlagBits::DEPTH | daxa::ImageAspectFlagBits::STENCIL,
            .size = { this->window->size_x, this->window->size_y, 1 },
            .mip_level_count = 1,
            .array_layer_count = 1,
            .sample_count = 1,
            .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT,
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY
        });

        this->draw_pipeline = this->context.pipeline_compiler.create_raster_pipeline({
            .vertex_shader_info = {.source = daxa::ShaderFile{"draw.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_VERT"}}}},
            .fragment_shader_info = {.source = daxa::ShaderFile{"draw.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_FRAG"}}}},
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
    }

    RenderingSystem::~RenderingSystem() {
        this->context.device.wait_idle();
        this->context.device.collect_garbage();
        this->context.device.destroy_image(depth_image);
        ImGui_ImplGlfw_Shutdown();
    }

    void RenderingSystem::draw(const std::shared_ptr<Scene>& scene, ControlledCamera3D& camera) {
        auto swapchain_image = this->context.swapchain.acquire_next_image();
        if (swapchain_image.is_empty())
        {
            return;
        }

        u32 size_x = this->context.swapchain.get_surface_extent().x;
        u32 size_y = this->context.swapchain.get_surface_extent().y;

        auto cmd_list = this->context.device.create_command_list({
            .debug_name = APPNAME_PREFIX("cmd_list"),
        });

        {
            glm::mat4 view = camera.camera.get_view();

            CameraInfo camera_info {
                .projection_matrix = *reinterpret_cast<const f32mat4x4*>(&camera.camera.proj_mat),
                .view_matrix = *reinterpret_cast<const f32mat4x4*>(&view),
                .position = *reinterpret_cast<const f32vec3*>(&camera.pos)
            };

            camera_buffer->update(cmd_list, camera_info);
        }

        cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::COLOR_ATTACHMENT_OUTPUT_WRITE,
            .before_layout = daxa::ImageLayout::UNDEFINED,
            .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
            .image_id = swapchain_image,
        });

        cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
            .before_layout = daxa::ImageLayout::UNDEFINED,
            .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
            .image_slice = {.image_aspect = daxa::ImageAspectFlagBits::DEPTH | daxa::ImageAspectFlagBits::STENCIL},
            .image_id = depth_image,
        });

        f32 factor = 32.0f;
        cmd_list.begin_renderpass({
            .color_attachments = {{
                .image_view = swapchain_image.default_view(),
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = std::array<f32, 4>{0.2f / factor, 0.4f / factor, 1.0f / factor, 1.0f},
            }},
            .depth_attachment = {{
                .image_view = depth_image.default_view(),
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = daxa::DepthValue{1.0f, 0},
            }},
            .render_area = {.x = 0, .y = 0, .width = size_x, .height = size_y},
        });

        cmd_list.set_viewport({
            .x = 0.0f,
            .y = static_cast<f32>(size_y),
            .width = static_cast<f32>(size_x),
            .height = -static_cast<f32>(size_y),
            .min_depth = 0.0f,
            .max_depth = 1.0f 
        });
        cmd_list.set_pipeline(draw_pipeline);

        scene->iterate([&](Entity entity){
            if(entity.has_component<ModelComponent>()) {
                auto& model = entity.get_component<ModelComponent>().model;

                DrawPush push_constant;
                push_constant.camera_info_buffer = camera_buffer->buffer_address;
                push_constant.object_info_buffer = entity.get_component<TransformComponent>().object_info->buffer_address;
                push_constant.lights_info_buffer = scene->lights_buffer->buffer_address;
                push_constant.irradiance_map = this->ibl_renderer->irradiance_cube;
                push_constant.brdfLUT = this->ibl_renderer->BRDFLUT;
                push_constant.prefilter_map = this->ibl_renderer->prefiltered_cube;

                model->bind_index_buffer(cmd_list);
                model->draw(cmd_list, push_constant);
            }
        });

        this->ibl_renderer->draw(cmd_list, camera.camera.proj_mat, camera.camera.get_view());

        cmd_list.end_renderpass();

        imgui_renderer.record_commands(ImGui::GetDrawData(), cmd_list, swapchain_image, size_x, size_y);

        cmd_list.pipeline_barrier_image_transition({
            .awaited_pipeline_access = daxa::AccessConsts::ALL_GRAPHICS_READ_WRITE,
            .before_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
            .after_layout = daxa::ImageLayout::PRESENT_SRC,
            .image_id = swapchain_image,
        });

        cmd_list.complete();

        this->context.device.submit_commands({
            .command_lists = {std::move(cmd_list)},
            .wait_binary_semaphores = {this->context.swapchain.get_acquire_semaphore()},
            .signal_binary_semaphores = {this->context.swapchain.get_present_semaphore()},
            .signal_timeline_semaphores = {{this->context.swapchain.get_gpu_timeline_semaphore(), this->context.swapchain.get_cpu_timeline_value()}},
        });

        this->context.device.present_frame({
            .wait_binary_semaphores = {this->context.swapchain.get_present_semaphore()},
            .swapchain = this->context.swapchain,
        });
    }

    void RenderingSystem::resize() {
        this->context.swapchain.resize();
        this->context.device.destroy_image(this->depth_image);
        this->depth_image = this->context.device.create_image({
            .format = daxa::Format::D24_UNORM_S8_UINT,
            .aspect = daxa::ImageAspectFlagBits::DEPTH | daxa::ImageAspectFlagBits::STENCIL,
            .size = {this->context.swapchain.get_surface_extent().x, this->context.swapchain.get_surface_extent().y, 1},
            .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT,
        });
    }
}