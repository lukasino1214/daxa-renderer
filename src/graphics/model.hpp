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
    uint32_t first_index;
    uint32_t first_vertex;
    uint32_t index_count;
    uint32_t vertex_count;
    Texture albedo_texture;
    Texture metallic_roughness_texture;
    Texture normal_map_texture;
};

struct Model {
    daxa::BufferId vertex_buffer;
    daxa::BufferId index_buffer;
    std::vector<Primitive> primitives;
    std::vector<Texture> images;
    Texture default_texture;
    u64 vertex_buffer_address;

    static Model load(daxa::Device & device, const std::filesystem::path & path);

    void bind_index_buffer(daxa::CommandList & cmd_list);
    void draw(daxa::CommandList & cmd_list, const glm::mat4 & mvp, const glm::vec3& camera_position, u64 camera_info_buffer, u64 object_info_buffer, u64 lights_info_buffer);
    
    void destroy(daxa::Device & device);
};