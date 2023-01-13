#pragma once

#include <daxa/daxa.hpp>
#include <glm/glm.hpp>
#include <vector>

using namespace daxa::types;
#include "../../shaders/shared.inl"
#include "texture.hpp"

namespace dare {
    struct Primitive {
        u32 first_index;
        u32 first_vertex;
        u32 index_count;
        u32 vertex_count;
        u32 material_index;
    };

    struct Model {
        daxa::BufferId vertex_buffer;
        daxa::BufferId index_buffer;
        std::vector<Primitive> primitives;
        std::vector<MaterialInfo> material_infos;
        std::vector<daxa::BufferId> material_buffers;
        std::vector<u64> material_buffer_addresses;
        std::vector<std::unique_ptr<Texture>> images;
        std::unique_ptr<Texture> default_texture;
        u64 vertex_buffer_address;
        daxa::Device device;
        std::string path;

        Model(daxa::Device device, const std::string& path);
        ~Model();

        void bind_index_buffer(daxa::CommandList& cmd_list);
        void draw(daxa::CommandList& cmd_list);
        void draw(daxa::CommandList& cmd_list, DrawPush& push_constant);
    };
}