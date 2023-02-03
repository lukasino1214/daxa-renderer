#pragma once

#include <daxa/daxa.hpp>
using namespace daxa::types;
#include <daxa/utils/pipeline_manager.hpp>
#include <glm/glm.hpp>
#include "../data/entity.hpp"
#include "../data/scene.hpp"
#include "../graphics/buffer.hpp"


namespace dare {
    struct LightManager {
        LightManager(std::shared_ptr<Scene> scene, daxa::Device& device, daxa::PipelineManager& pipeline_manager);
        ~LightManager();

        void reload();
        void update(daxa::CommandList& cmd_list, daxa::SamplerId& sampler);

        void render_normal_shadow_map(daxa::CommandList& cmd_list, const glm::mat4& vp, glm::ivec2 size, daxa::ImageId depth_image);
        void render_variance_shadow_map(daxa::CommandList& cmd_list, const glm::mat4& vp, glm::ivec2 size, daxa::ImageId depth_image, daxa::ImageId temp_image, daxa::ImageId variance_image);
        void render_variance_shadow_map(daxa::CommandList& cmd_list, const glm::mat4& vp, glm::ivec2 size, daxa::ImageId depth_image, daxa::ImageId temp_image, daxa::ImageId variance_image, daxa::ImageViewId face, f32vec3 light_position, f32 far_plane);

        daxa::Device& device;

        std::unique_ptr<Buffer<LightsInfo>> lights_buffer;
        std::shared_ptr<Scene> scene;

        std::shared_ptr<daxa::RasterPipeline> normal_shadow_pipeline;
        std::shared_ptr<daxa::RasterPipeline> variance_shadow_pipeline;
        std::shared_ptr<daxa::RasterPipeline> variance_shadow_point_pipeline;
        std::shared_ptr<daxa::RasterPipeline> filter_gauss_pipeline;
    };
}