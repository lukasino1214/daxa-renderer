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
        ModelManager(daxa::Device& device);
        ~ModelManager();

        void reallocate_buffers(u32 vertex_count, u32 index_count);
        auto load_model(const std::string& path) -> std::shared_ptr<Model>;

        daxa::Device& device;
        daxa::BufferId vertex_buffer;
        daxa::BufferId index_buffer;

        u32 last_vertex_count = 0;        
        u32 last_index_count = 0;
        u32 allocated_vertex_count = STARTING_COUNT;
        u32 allocated_index_count = STARTING_COUNT;
    };
}