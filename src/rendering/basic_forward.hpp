#pragma once

#include "task.hpp"

namespace dare {
    struct BasicForward: public Task {
        struct Settings {
            struct Texturing {
                bool none = true;
                bool vertex_color = false;
                bool albedo = false;
            } texturing;

            struct ShadingModel {
                bool none = true;
                bool lambertian = false;
                bool phong = false;
                bool blinn_phong = false;
                bool gourad = false;
                bool gaussian = false;
            } shading_model;
        } settings;

        BasicForward(RenderContext& context);
        virtual ~BasicForward() override;

        virtual void render(daxa::CommandList& cmd_list, const std::shared_ptr<Scene>& scene, daxa::BufferDeviceAddress camera_buffer) override;
        virtual void resize(u32 sx, u32 sy) override;

        virtual void render_settings_ui() override;
        virtual auto settings_to_string()-> std::string override;
        virtual void rebuild_pipeline() override;

        virtual auto get_color_image() -> daxa::ImageId override { return this->color_image; }
        virtual auto get_depth_image() -> daxa::ImageId override { return this->depth_image; }

        daxa::ImageId color_image;
        daxa::ImageId depth_image;

        daxa::RasterPipeline draw_pipeline;
        bool has_rebuild_pipeline = true;
    };
}