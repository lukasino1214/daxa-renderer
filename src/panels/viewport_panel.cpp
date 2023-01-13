#include "viewport_panel.hpp"

namespace dare {
    void ViewportPanel::draw(daxa::ImageId image) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Viewport");

        ImVec2 region = ImGui::GetContentRegionAvail();
        if(region.x != size.x || region.y != size.y) {
            size = { region.x, region.y };
            resized = true;
        } else {
            resized = false;
        }

        ImGui::Image(*reinterpret_cast<ImTextureID const *>(&image), region);
        ImGui::End();
        ImGui::PopStyleVar();
    }

    auto ViewportPanel::get_size() -> glm::vec2 {
        return size;
    }

    auto ViewportPanel::should_resize() -> bool {
        return resized;
    }
}