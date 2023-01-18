#pragma once

#include <daxa/daxa.hpp>
using namespace daxa::types;

#include "graphics/texture.hpp"

#include <cassert>
#include <fastgltf_types.hpp>

/*struct Primitive {
    u32 first_index;
    u32 first_vertex;
    u32 index_count;
    u32 vertex_count;
    u32 material_index;
};

struct Mesh {
    std::vector<Primitive> primitives;
};*/

using namespace dare;

struct NewModel {
    NewModel(daxa::Device device, const std::string& _path);
    ~NewModel();

    void load_image(const std::unique_ptr<fastgltf::Asset>& asset, fastgltf::Image& image);
    void load_material(const std::unique_ptr<fastgltf::Asset>& asset, fastgltf::Material& material);
    void load_mesh(const std::unique_ptr<fastgltf::Asset>& asset, fastgltf::Mesh& mesh);

    std::vector<std::unique_ptr<Texture>> images;
    std::unique_ptr<Texture> default_texture;
    //std::vector<Mesh> meshes;

    daxa::Device device;
};