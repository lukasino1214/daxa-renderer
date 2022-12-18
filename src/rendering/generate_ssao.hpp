#pragma once

#include <daxa/daxa.hpp>
using namespace daxa::types;

namespace dare {
    namespace SSAO {
        struct SSAOData {
            daxa::ImageId ssao_noise;
            daxa::BufferId ssao_kernel;
        };

        auto generate(daxa::Device& device) -> SSAOData;
        void cleanup(daxa::Device& device, SSAOData& data);
    }
}