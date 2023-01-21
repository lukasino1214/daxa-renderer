#pragma once

#include <daxa/daxa.hpp>
using namespace daxa::types;
#include <glm/glm.hpp>
#include <daxa/utils/pipeline_manager.hpp>

namespace dare {
    struct SSAORenderer {
        SSAORenderer(daxa::PipelineManager& pipeline_manager, daxa::Device device, const glm::ivec2& half_size);
        ~SSAORenderer();

        void render(daxa::CommandList& cmd_list, daxa::ImageId& normal_image, daxa::ImageId depth_image, daxa::BufferDeviceAddress camera_buffer_address, daxa::SamplerId& sampler);
        void resize(const glm::ivec2& size);

        std::shared_ptr<daxa::RasterPipeline> ssao_generation_pipeline;
        std::shared_ptr<daxa::RasterPipeline> ssao_blur_pipeline;

        daxa::ImageId ssao_image;
        daxa::ImageId ssao_blur_image;

        daxa::Device device;
        glm::ivec2 half_size;
    };
}