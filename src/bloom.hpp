#pragma once
#include <daxa/daxa.hpp>
using namespace daxa::types;
#include <glm/glm.hpp>
#include <daxa/utils/pipeline_manager.hpp>

struct BloomMip {
	glm::vec2 size;
	glm::ivec2 int_size;
	daxa::ImageId texture;
    daxa::Device device;

    BloomMip(daxa::Device device, const glm::vec2& size, const glm::ivec2& int_size);
    ~BloomMip();
};

struct BloomFBO {
    BloomFBO(daxa::Device device, const glm::ivec2& window_size, u32 mip_chain_length);
    ~BloomFBO();

    daxa::Device device;
    std::vector<std::unique_ptr<BloomMip>> mip_chain;
};

struct BloomRenderer {
    BloomRenderer(daxa::PipelineManager& pipeline_manager, daxa::Device device, const glm::ivec2& window_size);
    ~BloomRenderer();

    void render(daxa::CommandList& cmd_list, daxa::ImageId& src_image, daxa::SamplerId& sampler);

    void render_downsamples(daxa::CommandList& cmd_list, daxa::ImageId& src_image, daxa::SamplerId& sampler);
    void render_upsamples(daxa::CommandList& cmd_list, daxa::ImageId& src_image, daxa::SamplerId& sampler);

    std::unique_ptr<BloomFBO> bloom_mips;
    std::shared_ptr<daxa::RasterPipeline> down_sample_pipeline;
    std::shared_ptr<daxa::RasterPipeline> up_sample_pipeline;
    daxa::Device device;
    glm::ivec2 window_size;
};