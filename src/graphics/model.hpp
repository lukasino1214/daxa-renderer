#pragma once

#include <daxa/daxa.hpp>
#include <glm/glm.hpp>
#include <vector>

using namespace daxa::types;
#include "../../shaders/shared.inl"
#include "texture.hpp"

namespace dare {
    struct Primitive {
        u32 position_offset = 0;
        u32 normal_offset = 0;
        u32 uv_offset = 0;
        u32 vertex_count = 0;
        u32 vertex_offset = 0;        
        u32 index_count = 0;
        u32 index_offset = 0;
        u32 material_index = 0;
    };

    struct ModelManager;

    struct Model {
        u32 position_offset;
        u32 normal_offset;
        u32 uv_offset;
        u32 index_offset;
        u32 vertex_offset;

        std::vector<Primitive> primitives;
        std::vector<daxa::BufferId> material_buffers;
        std::vector<std::unique_ptr<Texture>> images;
        std::unique_ptr<Texture> default_texture;
        daxa::Device device;
        std::string path;
        ModelManager* model_manager;

        struct LoadingResult {
            std::shared_ptr<Model> model;
            std::vector<glm::vec3> positions;            
            std::vector<glm::vec3> normals;
            std::vector<glm::vec2> uvs;
            std::vector<u32> indices;
        };


        Model();
        ~Model();

        static auto load_model(daxa::Device device, const std::string& path) -> LoadingResult;

        void draw(daxa::CommandList& cmd_list);
        void draw(daxa::CommandList& cmd_list, DrawPush& push_constant);
        void draw(daxa::CommandList& cmd_list, ShadowPush& push_constant);
    };
}