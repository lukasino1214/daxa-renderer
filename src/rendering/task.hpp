#pragma once

#include <glm/glm.hpp>
#include <daxa/daxa.hpp>
using namespace daxa::types;
#include "render_context.hpp"
#include "../data/scene.hpp"
#include "../data/entity.hpp"
#include "../data/components.hpp"

namespace dare {
    struct Task { 
        Task(RenderContext& context);
        virtual ~Task();

        virtual void render(daxa::CommandList& cmd_list, const std::shared_ptr<Scene>& scene, daxa::BufferDeviceAddress camera_buffer) = 0;
        virtual void resize(u32 sx, u32 sy) = 0;

        virtual void render_settings_ui() = 0;
        virtual auto settings_to_string()-> std::string = 0;
        virtual void rebuild_pipeline() = 0;

        virtual auto get_color_image() -> daxa::ImageId = 0;
        virtual auto get_depth_image() -> daxa::ImageId = 0;

    protected:
        glm::vec2 size{800, 600};
        RenderContext& context;
    };
}