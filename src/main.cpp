
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
#include "panels/viewport_panel.hpp"

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
        
        u64 cpu_framecount = 0;

        u32 current_camera = 0;
        std::vector<ControlledCamera3D> cameras{2};

        bool paused = true;

        std::shared_ptr<Scene> scene = SceneSerializer::deserialize(rendering_system->context.device, "test.scene");
        std::shared_ptr<SceneHiearchyPanel> scene_hiearchy = std::make_shared<SceneHiearchyPanel>(scene);
        std::unique_ptr<ViewportPanel> viewport_panel = std::make_unique<ViewportPanel>();

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

            {
                ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
                ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground;
                const ImGuiViewport *viewport = ImGui::GetMainViewport();
                ImGui::SetNextWindowPos(viewport->WorkPos);
                ImGui::SetNextWindowSize(viewport->WorkSize);
                ImGui::SetNextWindowViewport(viewport->ID);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
                window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
                window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
                ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
                ImGui::Begin("DockSpace Demo", nullptr, window_flags);
                ImGui::PopStyleVar(3);
                ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
                ImGui::End();
            }
            
            // Debug
            {
                ImGui::Begin("Debug");
                ImGui::Text("CPU Frame Count: %i", static_cast<i32>(cpu_framecount));
                ImGui::Text("Frame time: %f", delta_time);
                ImGui::Text("Frame Per Second: %f", 1.0 / delta_time);
                if(ImGui::Button("Save scene")) {
                    SceneSerializer::serialize(scene, "test.scene");
                }
                if(ImGui::Button("Load scene")) {
                    scene = SceneSerializer::deserialize(this->rendering_system->context.device, "test.scene");
                    scene_hiearchy = std::make_shared<SceneHiearchyPanel>(scene);
                }
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
            viewport_panel->draw(rendering_system->get_render_image());

            rendering_system->task->render_settings_ui();

            ImGui::Render();
        }

        void draw() {
            current_frame = glfwGetTime();
            delta_time = current_frame - last_frame;
            last_frame = current_frame;

            ui_update();
            this->rendering_system->task->rebuild_pipeline();

            scene->update();

            if(viewport_panel->should_resize()) {
                glm::vec2 size = viewport_panel->get_size();
                cameras[current_camera].camera.resize(static_cast<i32>(size.x), static_cast<i32>(size.y));
                on_resize(size.x, size.y);
            }

            cameras[current_camera].camera.set_pos(cameras[current_camera].pos);
            cameras[current_camera].camera.set_rot(cameras[current_camera].rot.x, cameras[current_camera].rot.y);
            cameras[current_camera].update(delta_time);

            rendering_system->draw(scene, cameras[current_camera]);
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

            ImGui_ImplGlfw_InitForVulkan(window->glfw_window_ptr, true);
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
                rendering_system->resize(sx, sy);
                size_x = rendering_system->context.swapchain.get_surface_extent().x;
                size_y = rendering_system->context.swapchain.get_surface_extent().y;
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