#pragma once

#include <daxa/utils/imgui.hpp>
#include <imgui_impl_glfw.h>

#include "../rendering/render_context.hpp"
#include "../graphics/window.hpp"
#include "../data/scene.hpp"
#include "../graphics/camera.hpp"
#include "../graphics/buffer.hpp"
#include "../rendering/task.hpp"

namespace dare {
    struct RenderingSystem {
        RenderContext context;
        std::unique_ptr<Window>& window;
        daxa::ImGuiRenderer imgui_renderer;

        std::unique_ptr<Task> task;
        std::unique_ptr<Buffer<CameraInfo>> camera_buffer;

        glm::vec2 size = { 400.0f, 300.0f };

        RenderingSystem(std::unique_ptr<Window>& window);
        ~RenderingSystem();

        void draw(const std::shared_ptr<Scene>& scene, ControlledCamera3D& camera);
        void resize(u32 sx, u32 sy);

        auto get_render_image() -> daxa::ImageId;
    };
}