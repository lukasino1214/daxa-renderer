
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
#include "systems/rendering_system.hpp"

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

        std::unique_ptr<RenderingSystem> rendering_system = std::make_unique<RenderingSystem>(window);

        static inline constexpr u64 FRAMES_IN_FLIGHT = 1;
        u64 cpu_framecount = FRAMES_IN_FLIGHT - 1;

        u32 current_camera = 0;
        std::vector<ControlledCamera3D> cameras{2};

        Buffer<CameraInfo> camera_info_buffer = Buffer<CameraInfo>(rendering_system->context.device);
        Buffer<LightsInfo> lights_info_buffer = Buffer<LightsInfo>(rendering_system->context.device);

        bool paused = true;

        ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

        u32 viewport_size_x = 1280, viewport_size_y = 720;
        bool resized = true;

        std::shared_ptr<Scene> scene;
        std::shared_ptr<SceneHiearchyPanel> scene_hiearchy;

        App() = default;
        ~App() = default;

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

            scene_hiearchy->draw();
            ImGui::Render();
        }

        void draw() {
            current_frame = glfwGetTime();
            delta_time = current_frame - last_frame;
            last_frame = current_frame;

            ui_update();

            {
                auto cmd_list = rendering_system->context.device.create_command_list({
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
                rendering_system->context.device.submit_commands({
                    .command_lists = {std::move(cmd_list)}
                });
            }

            cameras[current_camera].camera.resize(static_cast<i32>(size_x), static_cast<i32>(size_y));
            cameras[current_camera].camera.set_pos(cameras[current_camera].pos);
            cameras[current_camera].camera.set_rot(cameras[current_camera].rot.x, cameras[current_camera].rot.y);
            cameras[current_camera].update(delta_time);

            /*if (pipeline_compiler.check_if_sources_changed(draw_pipeline)) {
                auto new_pipeline = pipeline_compiler.recreate_raster_pipeline(draw_pipeline);
                std::cout << new_pipeline.to_string() << std::endl;
                if (new_pipeline.is_ok()) {
                    draw_pipeline = new_pipeline.value();
                }
            }*/

            rendering_system->draw(scene, cameras[current_camera], camera_info_buffer, lights_info_buffer);
        }

        void setup() {
            glfwSetWindowUserPointer(window->glfw_window_ptr, this);
            glfwSetCursorPosCallback(window->glfw_window_ptr, [](GLFWwindow * window_ptr, f64 x, f64 y) {
                auto & app = *reinterpret_cast<App *>(glfwGetWindowUserPointer(window_ptr));
                app.on_mouse_move(static_cast<f32>(x), static_cast<f32>(y));
            });
            glfwSetMouseButtonCallback(window->glfw_window_ptr, [](GLFWwindow * window_ptr, i32 button, i32 action, i32) {
                auto & app = *reinterpret_cast<App *>(glfwGetWindowUserPointer(window_ptr));
                app.on_mouse_button(button, action);
            });
            glfwSetKeyCallback(window->glfw_window_ptr, [](GLFWwindow * window_ptr, i32 key, i32, i32 action, i32) {
                auto & app = *reinterpret_cast<App *>(glfwGetWindowUserPointer(window_ptr));
                app.on_key(key, action);
            });
            glfwSetFramebufferSizeCallback(window->glfw_window_ptr, [](GLFWwindow * window_ptr, i32 w, i32 h) {
                auto & app = *reinterpret_cast<App *>(glfwGetWindowUserPointer(window_ptr));
                app.on_resize(static_cast<u32>(w), static_cast<u32>(h));
            });

            scene = std::make_shared<Scene>();

            scene = SceneSerializer::deserialize(rendering_system->context.device, "test.scene");
            std::cout << "Loading models done!" << std::endl;

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
                rendering_system->context.swapchain.resize();
                size_x = rendering_system->context.swapchain.get_surface_extent().x;
                size_y = rendering_system->context.swapchain.get_surface_extent().y;
                rendering_system->context.device.destroy_image(rendering_system->depth_image);
                rendering_system->depth_image = rendering_system->context.device.create_image({
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