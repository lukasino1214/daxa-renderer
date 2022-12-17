#pragma once

#include "task.hpp"

namespace dare {
    struct BasicDeffered : public Task {
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
                bool gaussian = false;
            } shading_model;

            struct NormalMapping {
                bool none = true;
                bool using_tangents = false;
                bool calculating_TBN_vectors = false;
                bool reorthogonalize_TBN_vectors  = false;
            } normal_mappings;

            struct VisualizeAttachments {
                bool none = true;
                bool albedo = false;
                bool normal = false;
                bool position  = false;
            } visualize_attachments;
        } settings;

        BasicDeffered(RenderContext& context);
        virtual ~BasicDeffered() override;

        virtual void render(daxa::CommandList& cmd_list, const std::shared_ptr<Scene>& scene, daxa::BufferDeviceAddress camera_buffer) override;
        virtual void resize(u32 sx, u32 sy) override;

        virtual void render_settings_ui() override;
        virtual auto settings_to_string()-> std::string override;
        virtual void rebuild_pipeline() override;

        virtual auto get_color_image() -> daxa::ImageId override { return this->color_image; }
        virtual auto get_depth_image() -> daxa::ImageId override { return this->depth_image; }

        daxa::ImageId color_image;
        daxa::ImageId albedo_image;
        daxa::ImageId normal_image;
        daxa::ImageId position_image;
        daxa::ImageId depth_image;

        daxa::SamplerId sampler;

        daxa::RasterPipeline g_buffer_gather_pipeline;
        daxa::RasterPipeline composition_pipeline;
        bool has_rebuild_pipeline = true;        
    };
}