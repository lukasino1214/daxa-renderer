#pragma once

#include <daxa/daxa.hpp>

using namespace daxa::types;
#include "../../shaders/shared.inl"

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include "texture.hpp"

using namespace daxa::types;

#define APPNAME "Daxa Template App"
#define APPNAME_PREFIX(x) ("[" APPNAME "] " x)

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

    static Model load(daxa::Device& device, const std::filesystem::path& path);

    void bind_index_buffer(daxa::CommandList& cmd_list);
    void draw(daxa::CommandList& cmd_list);
    void draw(daxa::CommandList& cmd_list, DrawPush& push_constant);
    
    void destroy(daxa::Device& device);
};