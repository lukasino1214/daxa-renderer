#include "model.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define STBI_MSC_SECURE_CRT
#include <tiny_gltf.h>

Model Model::load(daxa::Device & device, const std::filesystem::path & path) {
    auto timer = std::chrono::system_clock::now();
    std::vector<DrawVertex> vertices{};
    std::vector<u32> indices{};
    std::vector<Primitive> primitives{};
    std::vector<Texture> images{};

    std::string warn, err;
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    if (!loader.LoadASCIIFromFile(&model, &err, &warn, path.c_str())) {
        throw std::runtime_error("failed to load gltf file!");
    }

    auto get_image_type = [&](const tinygltf::Model& model, usize image_index) -> TextureType {
        for(auto& material : model.materials) {
            if(material.pbrMetallicRoughness.baseColorTexture.index != 1) {
                u32 diffuseTextureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
                const tinygltf::Texture& diffuseTexture = model.textures[diffuseTextureIndex];
                if (image_index == diffuseTexture.source) {
                    return TextureType::SRGB;
                }
            }
        }

        return TextureType::UNORM;
    };


    for(usize image_index = 0; image_index < model.images.size(); image_index++) {
        unsigned char* buffer;
        u64 buffer_size;
        tinygltf::Image& image = model.images[image_index];
        if (image.component == 3) {
            buffer_size = image.width * image.height * 4;
            std::vector<unsigned char> image_data(buffer_size, 0x00);

            buffer = (unsigned char*)image_data.data();
            unsigned char* rgba = buffer;
            unsigned char* rgb = &image.image[0];
            for (usize i = 0; i < image.width * image.height; ++i) {
                memcpy(rgba, rgb, sizeof(unsigned char) * 3);
                rgba += 4;
                rgb += 3;
            }
        }
        else {
            buffer = &image.image[0];
            buffer_size = image.image.size();
        }

        images.push_back(Texture::load(device, image.width, image.height, buffer, get_image_type(model, image_index)));
    }

    Texture default_texture = Texture::load(device, "assets/textures/white.png");

    for (auto & scene : model.scenes) {
        for (size_t i = 0; i < scene.nodes.size(); i++) {
            auto & node = model.nodes[i];
            uint32_t vertexOffset = 0;
            uint32_t indexOffset = 0;

            for (auto & primitive : model.meshes[node.mesh].primitives) {
                uint32_t vertexCount = 0;
                uint32_t indexCount = 0;

                const float* positionBuffer = nullptr;
                const float* normalBuffer = nullptr;
                const float* texCoordsBuffer = nullptr;

                if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
                    const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.find("POSITION")->second];
                    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                    positionBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    vertexCount = accessor.count;
                }

                if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                    const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.find("NORMAL")->second];
                    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                    normalBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }

                if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                    const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
                    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                    texCoordsBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }

                for (size_t v = 0; v < vertexCount; v++) {
                    glm::vec3 temp_position = glm::make_vec3(&positionBuffer[v * 3]);
                    glm::vec3 temp_normal = glm::make_vec3(&normalBuffer[v * 3]);
                    glm::vec2 temp_uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec2(0.0f);
                    DrawVertex vertex{
                        .position = {temp_position.x, temp_position.y, temp_position.z},
                        .normal = {temp_normal.x, temp_normal.y, temp_normal.z},
                        .uv = {temp_uv.x, temp_uv.y}};

                    vertices.push_back(vertex);
                }

                {
                    const tinygltf::Accessor & accessor = model.accessors[primitive.indices];
                    const tinygltf::BufferView & bufferView = model.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer & buffer = model.buffers[bufferView.buffer];

                    indexCount += static_cast<uint32_t>(accessor.count);

                    switch (accessor.componentType) {
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                        const uint32_t * buf = reinterpret_cast<const uint32_t *>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                        for (size_t index = 0; index < accessor.count; index++) {
                            indices.push_back(buf[index]);
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
                        const uint16_t * buf = reinterpret_cast<const uint16_t *>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                        for (size_t index = 0; index < accessor.count; index++) {
                            indices.push_back(buf[index]);
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
                        const uint8_t * buf = reinterpret_cast<const uint8_t *>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                        for (size_t index = 0; index < accessor.count; index++) {
                            indices.push_back(buf[index]);
                        }
                        break;
                    }
                    default: std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
                    }
                }

                Texture albedo_texture;
                Texture metallic_roughness_texture;
                Texture normal_map;

                if (primitive.material != -1) {
                    tinygltf::Material& material = model.materials[primitive.material];

                    //albedo
                    if (material.pbrMetallicRoughness.baseColorTexture.index != -1) {
                        uint32_t textureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
                        uint32_t imageIndex = model.textures[textureIndex].source;
                        albedo_texture = images[imageIndex];
                    } else {
                        albedo_texture = default_texture;
                    }

                    //metallic roughness
                    if (material.pbrMetallicRoughness.metallicRoughnessTexture.index != -1) {
                        uint32_t textureIndex = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
                        uint32_t imageIndex = model.textures[textureIndex].source;
                        metallic_roughness_texture = images[imageIndex];
                    } else {
                        metallic_roughness_texture = default_texture;
                    }

                    //normal map
                    if (material.normalTexture.index != -1) {
                        uint32_t textureIndex = material.normalTexture.index;
                        uint32_t imageIndex = model.textures[textureIndex].source;
                        normal_map = images[imageIndex];
                    } else {
                        normal_map = default_texture;
                    }
                } else {
                    albedo_texture = default_texture;
                    metallic_roughness_texture = default_texture;
                    normal_map = default_texture;
                }

                Primitive temp_primitive {
                    .first_index = indexOffset,
                    .first_vertex = vertexOffset,
                    .index_count = indexCount,
                    .vertex_count = vertexCount,
                    .albedo_texture = albedo_texture,
                    .metallic_roughness_texture = metallic_roughness_texture,
                    .normal_map_texture = normal_map
                };

                primitives.push_back(temp_primitive);

                vertexOffset += vertexCount;
                indexOffset += indexCount;
            }
        }
    }

    daxa::BufferId vertex_buffer = device.create_buffer(daxa::BufferInfo{
        .size = static_cast<u32>(sizeof(DrawVertex) * vertices.size()),
        .debug_name = APPNAME_PREFIX("vertex_buffer"),
    });

    daxa::BufferId index_buffer = device.create_buffer(daxa::BufferInfo{
        .size = static_cast<u32>(sizeof(u32) * indices.size()),
        .debug_name = APPNAME_PREFIX("idnex_buffer"),
    });

    {
        auto cmd_list = device.create_command_list({
            .debug_name = APPNAME_PREFIX("cmd_list"),
        });

        auto vertex_staging_buffer = device.create_buffer({
            .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            .size = static_cast<u32>(sizeof(DrawVertex) * vertices.size()),
            .debug_name = APPNAME_PREFIX("vertex_staging_buffer"),
        });
        cmd_list.destroy_buffer_deferred(vertex_staging_buffer);

        auto buffer_ptr = device.map_memory_as<DrawVertex>(vertex_staging_buffer);
        std::memcpy(buffer_ptr, vertices.data(), vertices.size() * sizeof(DrawVertex));
        device.unmap_memory(vertex_staging_buffer);

        cmd_list.pipeline_barrier({
            .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
            .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_READ,
        });

        cmd_list.copy_buffer_to_buffer({
            .src_buffer = vertex_staging_buffer,
            .dst_buffer = vertex_buffer,
            .size = static_cast<u32>(sizeof(DrawVertex) * vertices.size()),
        });

        cmd_list.pipeline_barrier({
            .awaited_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
            .waiting_pipeline_access = daxa::AccessConsts::VERTEX_SHADER_READ,
        });
        cmd_list.complete();
        device.submit_commands({
            .command_lists = {std::move(cmd_list)},
        });
    }

    {
        auto cmd_list = device.create_command_list({
            .debug_name = APPNAME_PREFIX("cmd_list"),
        });

        auto index_staging_buffer = device.create_buffer({
            .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            .size = static_cast<u32>(sizeof(u32) * indices.size()),
            .debug_name = APPNAME_PREFIX("index_staging_buffer"),
        });
        cmd_list.destroy_buffer_deferred(index_staging_buffer);

        auto buffer_ptr = device.map_memory_as<u32>(index_staging_buffer);
        std::memcpy(buffer_ptr, indices.data(), indices.size() * sizeof(u32));
        device.unmap_memory(index_staging_buffer);

        cmd_list.pipeline_barrier({
            .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
            .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_READ,
        });

        cmd_list.copy_buffer_to_buffer({
            .src_buffer = index_staging_buffer,
            .dst_buffer = index_buffer,
            .size = static_cast<u32>(sizeof(u32) * indices.size()),
        });

        cmd_list.pipeline_barrier({
            .awaited_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
            .waiting_pipeline_access = daxa::AccessConsts::VERTEX_SHADER_READ,
        });
        cmd_list.complete();
        device.submit_commands({
            .command_lists = {std::move(cmd_list)},
        });
    }

    std::cout << path << " loaded in " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - timer).count() << " ms!" << std::endl;

    return Model {
        .vertex_buffer = vertex_buffer,
        .index_buffer = index_buffer,
        .primitives = std::move(primitives),
        .images = std::move(images),
        .default_texture = default_texture,
        .vertex_buffer_address = device.buffer_reference(vertex_buffer)
    };
}

void Model::destroy(daxa::Device& device) {
    device.destroy_buffer(vertex_buffer);
    device.destroy_buffer(index_buffer);
    for (auto & image : images) {
        image.destroy(device);
    }
    default_texture.destroy(device);
}

void Model::bind_index_buffer(daxa::CommandList & cmd_list) {
    cmd_list.set_index_buffer(index_buffer, 0, 4);
}

void Model::draw(daxa::CommandList & cmd_list, const glm::mat4 & mvp, const glm::vec3& camera_position,  u64 camera_info_buffer, u64 object_info_buffer, u64 lights_info_buffer) {
    for (auto & primitive : primitives) {
        cmd_list.push_constant(DrawPush{
            .vp = *reinterpret_cast<const f32mat4x4*>(&mvp),
            .camera_position = *reinterpret_cast<const f32vec3*>(&camera_position),
             //.camera_info_buffer = camera_info_buffer,
            .object_info_buffer = object_info_buffer,
            .lights_info_buffer = lights_info_buffer,
            .face_buffer = vertex_buffer_address,
            .albedo = primitive.albedo_texture.get_texture_id(),
            .metallic_roughness = primitive.metallic_roughness_texture.get_texture_id(),
            .normal_map = primitive.normal_map_texture.get_texture_id()
        });

        if (primitive.index_count > 0) {
            cmd_list.draw_indexed({
                .index_count = primitive.index_count,
                .instance_count = 1,
                .first_index = primitive.first_index,
                .vertex_offset = primitive.first_vertex,
                .first_instance = 0,
            });
        } else {
            cmd_list.draw({
                .vertex_count = primitive.vertex_count,
                .instance_count = 1,
                .first_vertex = primitive.first_vertex,
                .first_instance = 0
            });
        }
    }
}