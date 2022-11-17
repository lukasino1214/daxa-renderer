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
        glm::vec2 get_size();
        bool should_resize();

        bool resized = true;
        glm::vec2 size{ 400.0f, 300.0f };
    };
}