#pragma once

#include <daxa/utils/imgui.hpp>
#include <imgui_impl_glfw.h>

#include "../rendering/render_context.hpp"
#include "../graphics/window.hpp"
#include "../data/scene.hpp"
#include "../graphics/camera.hpp"
#include "../graphics/buffer.hpp"

#include "ibl_renderer.hpp"

namespace dare {
    struct RenderingSystem {
        RenderContext context;
        std::unique_ptr<Window>& window;
        daxa::ImGuiRenderer imgui_renderer;

        std::unique_ptr<IBLRenderer> ibl_renderer;

        daxa::RasterPipeline draw_pipeline;

        daxa::ImageId depth_image = {};
        std::unique_ptr<Buffer<CameraInfo>> camera_buffer;

        RenderingSystem(std::unique_ptr<Window>& window);
        ~RenderingSystem();

        void draw(const std::shared_ptr<Scene>& scene, ControlledCamera3D& camera);
        void resize();
    };
}