#pragma once
#include <daxa/daxa.hpp>

namespace dare {
    struct RenderContext {
        daxa::Context context = {};
        daxa::Device device = {};
        daxa::Swapchain swapchain = {};
        daxa::PipelineCompiler pipeline_compiler = {};
    };
}