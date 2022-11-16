#pragma once

#include <daxa/utils/imgui.hpp>
#include <imgui_impl_glfw.h>

#include "../rendering/render_context.hpp"
#include "../graphics/window.hpp"
#include "../data/scene.hpp"

namespace dare {
    struct RenderingSystem {
        RenderContext context;
        std::unique_ptr<Window>& window;
        daxa::ImGuiRenderer imgui_renderer;

        RenderingSystem(std::unique_ptr<Window>& window);
        ~RenderingSystem();

        void draw(const std::shared_ptr<Scene>& scene);
    };
}