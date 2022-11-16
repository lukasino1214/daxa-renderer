
#include <daxa/daxa.hpp>
#include <thread>
#include <iostream>
#include <cmath>

#include <daxa/utils/imgui.hpp>
#include <imgui_impl_glfw.h>
#include <imgui.h>

using namespace daxa::types;
#include "../shaders/shared.inl"

using Clock = std::chrono::high_resolution_clock;

#include "graphics/model.hpp"
#include "graphics/buffer.hpp"
#include "graphics/camera.hpp"
#include "graphics/window.hpp"
#include "data/entity.hpp"
#include "data/scene_serializer.hpp"
#include "data/scene.hpp"
#include "systems/ibl_renderer.hpp"
#include "panels/scene_hiearchy.hpp"

#define DAXA_GLSL 1

// remind retarded reinterpret 
// ImGui::ImageButton(*reinterpret_cast<ImTextureID const *>(&current_brush.preview_thumbnail)
namespace dare {
    struct App {
        f64 current_frame = glfwGetTime();
        f64 last_frame = current_frame;
        f64 delta_time;
        u32 size_x = 800, size_y = 600;
        std::unique_ptr<Window> window = std::make_unique<Window>(APPNAME);

        daxa::Context daxa_ctx = daxa::create_context({
            .enable_validation = false,
        });
        
        daxa::Device device = daxa_ctx.create_device({
            .use_scalar_layout = true,
            .debug_name = APPNAME_PREFIX("device"),
        });

        daxa::Swapchain swapchain = device.create_swapchain({
            .native_window = window->get_native_handle(),
            .present_mode = daxa::PresentMode::DO_NOT_WAIT_FOR_VBLANK,
            .image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST,
            .debug_name = APPNAME_PREFIX("swapchain"),
        });

        daxa::ImageId depth_image = device.create_image({
            .dimensions = 2,
            .format = daxa::Format::D24_UNORM_S8_UINT,
            .aspect = daxa::ImageAspectFlagBits::DEPTH | daxa::ImageAspectFlagBits::STENCIL,
            .size = {size_x, size_y, 1},
            .mip_level_count = 1,
            .array_layer_count = 1,
            .sample_count = 1,
            .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT,
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY
        });

        daxa::PipelineCompiler pipeline_compiler = device.create_pipeline_compiler({
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
            .debug_name = APPNAME_PREFIX("pipeline_compiler"),
        });

        daxa::ImGuiRenderer imgui_renderer;
        auto create_imgui_renderer() -> daxa::ImGuiRenderer {
            ImGui::CreateContext();
            ImGui_ImplGlfw_InitForVulkan(window->glfw_window_ptr, true);
            return daxa::ImGuiRenderer({
                .device = device,
                .pipeline_compiler = pipeline_compiler,
                .format = swapchain.get_format(),
            });
        }

        // clang-format off
        daxa::RasterPipeline draw_pipeline = pipeline_compiler.create_raster_pipeline({
            .vertex_shader_info = {.source = daxa::ShaderFile{"draw.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_VERT"}}}},
            .fragment_shader_info = {.source = daxa::ShaderFile{"draw.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_FRAG"}}}},
            .color_attachments = {{.format = swapchain.get_format(), .blend = {.blend_enable = true, .src_color_blend_factor = daxa::BlendFactor::SRC_ALPHA, .dst_color_blend_factor = daxa::BlendFactor::ONE_MINUS_SRC_ALPHA}}},
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
        // clang-format on

        static inline constexpr u64 FRAMES_IN_FLIGHT = 1;
        u64 cpu_framecount = FRAMES_IN_FLIGHT - 1;

        u32 current_camera = 0;
        std::vector<ControlledCamera3D> cameras{2};

        Buffer<CameraInfo> camera_info_buffer = Buffer<CameraInfo>(device);
        Buffer<LightsInfo> lights_info_buffer = Buffer<LightsInfo>(device);

        std::unique_ptr<IBLRenderer> ibl_system = std::make_unique<IBLRenderer>(device, pipeline_compiler, swapchain.get_format());

        bool paused = true;

        ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

        u32 viewport_size_x = 1280, viewport_size_y = 720;
        bool resized = true;

        std::shared_ptr<Scene> scene;
        std::shared_ptr<SceneHiearchyPanel> scene_hiearchy;

        App() = default;

        ~App() {
            device.wait_idle();
            device.collect_garbage();
            device.destroy_image(depth_image);
            ImGui_ImplGlfw_Shutdown();
        }

        bool update() {
            glfwPollEvents();
            if (glfwWindowShouldClose(window->glfw_window_ptr)) {
                return true;
            }

            if (!window->minimized) {
                draw();
            } else {
                using namespace std::literals;
                std::this_thread::sleep_for(1ms);
            }

            return false;
        }

        void ui_update() {
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            
            // Debug
            {
                ImGui::Begin("Debug");
                ImGui::Text("CPU Frame Count: %i", static_cast<i32>(cpu_framecount));
                ImGui::Text("Frame time: %f", delta_time);
                ImGui::Text("Frame Per Second: %f", 1.0 / delta_time);
                ImGui::End();
            }

            // Cameras
            {
                ImGui::Begin("Camera");
                if(ImGui::Button("Add Camera")) {
                    cameras.push_back({});
                }

                for(usize i = 0; i < cameras.size(); i++) {
                    if(ImGui::Button((std::string("camera: ") + std::to_string(i + 1)).c_str())) {
                        current_camera = i;
                    }
                }
                ImGui::End();
            }

            // Scene Hiearchy

            /*{
                ImGui::Begin("Scene Hiearchy");
                scene->iterate([](Entity entity){
                    ImGui::Text("Name: %s", entity.get_component<TagComponent>().tag.c_str());
                });

                ImGui::End();
            }

*/
            scene_hiearchy->draw();
            ImGui::Render();
        }

        void draw() {
            current_frame = glfwGetTime();
            delta_time = current_frame - last_frame;
            last_frame = current_frame;

            ui_update();

            {
                auto cmd_list = device.create_command_list({
                    .debug_name = APPNAME_PREFIX("updating object info of entities"),
                });

                scene->iterate([&](Entity entity) {
                    auto& comp = entity.get_component<TransformComponent>();
                    if(comp.is_dirty) {
                        auto m = comp.calculate_matrix();
                        auto n = comp.calculate_normal_matrix();
                        comp.object_info->update(cmd_list, ObjectInfo {
                            .model_matrix = *reinterpret_cast<const f32mat4x4 *>(&m),
                            .normal_matrix = *reinterpret_cast<const f32mat4x4 *>(&n)
                        });

                        comp.is_dirty = false;
                    }
                });

                cmd_list.complete();
                device.submit_commands({
                    .command_lists = {std::move(cmd_list)}
                });
            }

            cameras[current_camera].camera.resize(static_cast<i32>(size_x), static_cast<i32>(size_y));
            cameras[current_camera].camera.set_pos(cameras[current_camera].pos);
            cameras[current_camera].camera.set_rot(cameras[current_camera].rot.x, cameras[current_camera].rot.y);
            cameras[current_camera].update(delta_time);

            if (pipeline_compiler.check_if_sources_changed(draw_pipeline)) {
                auto new_pipeline = pipeline_compiler.recreate_raster_pipeline(draw_pipeline);
                std::cout << new_pipeline.to_string() << std::endl;
                if (new_pipeline.is_ok()) {
                    draw_pipeline = new_pipeline.value();
                }
            }

            auto swapchain_image = swapchain.acquire_next_image();
            if (swapchain_image.is_empty())
            {
                return;
            }

            auto cmd_list = device.create_command_list({
                .debug_name = APPNAME_PREFIX("cmd_list"),
            });

            {
                glm::mat4 view = cameras[current_camera].camera.get_view();
                
                CameraInfo camera_info {
                    .projection_matrix = *reinterpret_cast<const f32mat4x4*>(&cameras[current_camera].camera.proj_mat),
                    .view_matrix = *reinterpret_cast<const f32mat4x4*>(&view),
                    .position = *reinterpret_cast<const f32vec3*>(&cameras[current_camera].pos)
                };

                camera_info_buffer.update(cmd_list, camera_info);
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
                    push_constant.camera_info_buffer = camera_info_buffer.buffer_address;
                    push_constant.object_info_buffer = entity.get_component<TransformComponent>().object_info->buffer_address;
                    push_constant.lights_info_buffer = lights_info_buffer.buffer_address;
                    push_constant.irradiance_map = ibl_system->irradiance_cube;
                    push_constant.brdfLUT = ibl_system->BRDFLUT;
                    push_constant.prefilter_map = ibl_system->prefiltered_cube;

                    model->bind_index_buffer(cmd_list);
                    model->draw(cmd_list, push_constant);
                }
            });

            ibl_system->draw(cmd_list, cameras[current_camera].camera.proj_mat, cameras[current_camera].camera.get_view());

            cmd_list.end_renderpass();

            imgui_renderer.record_commands(ImGui::GetDrawData(), cmd_list, swapchain_image, size_x, size_y);

            cmd_list.pipeline_barrier_image_transition({
                .awaited_pipeline_access = daxa::AccessConsts::ALL_GRAPHICS_READ_WRITE,
                .before_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                .after_layout = daxa::ImageLayout::PRESENT_SRC,
                .image_id = swapchain_image,
            });

            cmd_list.complete();

            cpu_framecount++;
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

        void setup() {
            glfwSetWindowUserPointer(window->glfw_window_ptr, this);
            glfwSetCursorPosCallback(
                window->glfw_window_ptr,
                [](GLFWwindow * window_ptr, f64 x, f64 y)
                {
                    auto & app = *reinterpret_cast<App *>(glfwGetWindowUserPointer(window_ptr));
                    app.on_mouse_move(static_cast<f32>(x), static_cast<f32>(y));
                });
            glfwSetMouseButtonCallback(
                window->glfw_window_ptr,
                [](GLFWwindow * window_ptr, i32 button, i32 action, i32)
                {
                    auto & app = *reinterpret_cast<App *>(glfwGetWindowUserPointer(window_ptr));
                    app.on_mouse_button(button, action);
                });
            glfwSetKeyCallback(
                window->glfw_window_ptr,
                [](GLFWwindow * window_ptr, i32 key, i32, i32 action, i32)
                {
                    auto & app = *reinterpret_cast<App *>(glfwGetWindowUserPointer(window_ptr));
                    app.on_key(key, action);
                });
            glfwSetFramebufferSizeCallback(
                window->glfw_window_ptr,
                [](GLFWwindow * window_ptr, i32 w, i32 h)
                {
                    auto & app = *reinterpret_cast<App *>(glfwGetWindowUserPointer(window_ptr));
                    app.on_resize(static_cast<u32>(w), static_cast<u32>(h));
                });

            imgui_renderer = create_imgui_renderer();

            scene = std::make_shared<Scene>();

            scene = SceneSerializer::deserialize(device, "test.scene");
            std::cout << "Loading models done!" << std::endl;

            Entity sponza;
            scene->iterate([&](Entity entity){
                if(entity.has_component<TagComponent>()) {
                    auto& comp = entity.get_component<TagComponent>();
                    if(comp.tag == "Sponza") {
                        sponza = entity;
                    }
                }
            });

            scene_hiearchy = std::make_shared<SceneHiearchyPanel>(scene);

            LightsInfo lights_info;
            lights_info.num_point_lights = 3;

            lights_info.point_lights[0] = PointLight {
                .position = { 0.0f, -1.0f, 0.0f },
                .color = { 1.0f, 0.0f, 0.0f },
                .intensity = 64.0f,
            };

            lights_info.point_lights[1] = PointLight {
                .position = { 0.0f, -1.0f, 20.0f },
                .color = { 0.0f, 0.0f, 1.0f },
                .intensity = 64.0f,
            };

            lights_info.point_lights[2] = PointLight {
                .position = { 0.0f, -1.0f, -20.0f },
                .color = { 0.0f, 1.0f, 0.0f },
                .intensity = 64.0f,
            };

            lights_info_buffer.update(lights_info);

            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        }

        void on_mouse_button(i32, i32) {}
        void on_mouse_move(f32 x, f32 y) {
            if (!paused) {
                f32 center_x = static_cast<f32>(size_x / 2);
                f32 center_y = static_cast<f32>(size_y / 2);
                auto offset = f32vec2{x - center_x, center_y - y};
                cameras[current_camera].on_mouse_move(offset.x, offset.y);
                window->set_mouse_pos(center_x, center_y);
            }
        }

        void on_key(i32 key, i32 action) {
            if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
                toggle_pause();
            }

            if (!paused) {
                cameras[current_camera].on_key(key, action);
            }
        }

        void on_resize(u32 sx, u32 sy) {
            window->minimized = (sx == 0 || sy == 0);
            if (!window->minimized) {
                swapchain.resize();
                size_x = swapchain.info().width;
                size_y = swapchain.info().height;
                device.destroy_image(depth_image);
                depth_image = device.create_image({
                    .format = daxa::Format::D24_UNORM_S8_UINT,
                    .aspect = daxa::ImageAspectFlagBits::DEPTH | daxa::ImageAspectFlagBits::STENCIL,
                    .size = {size_x, size_y, 1},
                    .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT,
                });
                draw();
            }
        }

        void toggle_pause() {
            window->set_mouse_capture(paused);
            paused = !paused;
        }
    };
}

int main() {
    dare::App app = {};
    app.setup();
    while (true) {
        if (app.update())
            break;
    }
}