#pragma once

#include <daxa/daxa.hpp>
#include <stb_image.h>

using namespace daxa::types;
#include "../../shaders/shared.inl"

#include "../graphics/model.hpp"

namespace dare {
    struct IBLRenderer {
        TextureId BRDFLUT;
        daxa::ImageId env_map_image;
        TextureId env_map;
        daxa::ImageId irradiance_cube_image;
        TextureId irradiance_cube;
        daxa::ImageId prefiltered_cube_image;
        TextureId prefiltered_cube;
        daxa::RasterPipeline skybox_pipeline;
        std::unique_ptr<Model> cube_model;
        daxa::Device& device;

        IBLRenderer(daxa::Device& device, daxa::PipelineCompiler& pipeline_compiler, daxa::Format swapchain_format);
        ~IBLRenderer();

        void draw(daxa::CommandList& cmd_list, const glm::mat4& proj, const glm::mat4& view);
    };
}