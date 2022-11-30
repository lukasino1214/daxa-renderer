#pragma once

#include <daxa/utils/imgui.hpp>
#include <imgui_impl_glfw.h>
#include <imgui.h>
#include <glm/glm.hpp>
#include <daxa/daxa.hpp>

namespace dare {
    struct ViewportPanel {
        ViewportPanel() = default;
        ~ViewportPanel() = default;

        void draw(daxa::ImageId image);
        auto get_size() -> glm::vec2;
        auto should_resize() -> bool;

        bool resized = true;
        glm::vec2 size{ 400.0f, 300.0f };
    };
}