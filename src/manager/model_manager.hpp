#pragma once

#include <daxa/daxa.hpp>
using namespace daxa::types;
#include <glm/glm.hpp>
#include "../../shaders/shared.inl"
#include "../graphics/model.hpp"

namespace dare {
    static constexpr inline u32 VERTEX_SIZE = sizeof(DrawVertex);
    static constexpr inline u32 INDEX_SIZE = sizeof(u32);
    static constexpr inline u32 STARTING_COUNT = 32768;

    struct ModelManager {
        struct Buffer {
            daxa::BufferId buffer;
            u32 last_element_count = 0;
            u32 allocated_element_count = STARTING_COUNT;
            u32 element_size = 0;
        };

        ModelManager(daxa::Device& device);
        ~ModelManager();

        void copy_data_to(Buffer& buffer, u32 count, void* data);
        void reallocate_buffer(Buffer& buffer, u32 new_count);
        auto load_model(const std::string& path) -> std::shared_ptr<Model>;

        daxa::Device& device;

        Buffer position_buffer;
        Buffer normal_buffer;
        Buffer uv_buffer;        
        Buffer index_buffer;
    };
}