#include "model.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define STBI_MSC_SECURE_CRT
#include <tiny_gltf.h>

namespace dare {
    Model::Model(daxa::Device& device, const std::filesystem::path& path) : device{device}, path{path} {
        auto timer = std::chrono::system_clock::now();
        std::vector<DrawVertex> vertices{};
        std::vector<u32> indices{};

        std::string warn, err;
        tinygltf::TinyGLTF loader;
        tinygltf::Model model;
        if (!loader.LoadASCIIFromFile(&model, &err, &warn, path.c_str())) {
            throw std::runtime_error("failed to load gltf file!");
        }

        auto get_image_type = [&](const tinygltf::Model& model, usize image_index) -> TextureType {
            for(auto& material : model.materials) {
                if(material.pbrMetallicRoughness.baseColorTexture.index != -1) {
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

            std::unique_ptr<Texture> texture = std::make_unique<Texture>(device, image.width, image.height, buffer, get_image_type(model, image_index));
            images.push_back(std::move(texture));
        }

        default_texture = std::make_unique<Texture>(device, "assets/textures/white.png");

        for(usize material_index = 0; material_index < model.materials.size(); material_index++) {
            tinygltf::Material& material = model.materials[material_index];
            MaterialInfo material_info {};

            if(material.pbrMetallicRoughness.baseColorTexture.index != -1) {
                u32 texture_index = material.pbrMetallicRoughness.baseColorTexture.index;
                u32 image_index = model.textures[texture_index].source;
                material_info.albedo = images[image_index]->get_texture_id();
                material_info.albedo_factor = { 1.0f, 1.0f, 1.0f, 1.0f };
                material_info.has_albedo = 1;
            } else {
                material_info.albedo = default_texture->get_texture_id();
                std::vector<f64>& vector = material.pbrMetallicRoughness.baseColorFactor;
                material_info.albedo_factor = { static_cast<f32>(vector[0]), static_cast<f32>(vector[1]), static_cast<f32>(vector[2]), static_cast<f32>(vector[3]) };
                material_info.has_albedo = 0;
            }

            if(material.pbrMetallicRoughness.metallicRoughnessTexture.index != -1) {
                u32 texture_index = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
                u32 image_index = model.textures[texture_index].source;
                material_info.metallic_roughness = images[image_index]->get_texture_id();
                material_info.has_metallic_roughness = 1;
                material_info.metallic = 1.0f;
                material_info.roughness = 1.0f;
            } else {
                material_info.metallic_roughness = default_texture->get_texture_id();
                material_info.has_metallic_roughness = 0;
                material_info.metallic = static_cast<f32>(material.pbrMetallicRoughness.metallicFactor);
                material_info.roughness = static_cast<f32>(material.pbrMetallicRoughness.roughnessFactor);
            }

            if(material.normalTexture.index != -1) {
                u32 texture_index = material.normalTexture.index;
                u32 image_index = model.textures[texture_index].source;
                material_info.normal_map = images[image_index]->get_texture_id();
                material_info.has_normal_map = 1;
            } else {
                material_info.normal_map = default_texture->get_texture_id();
                material_info.has_normal_map = 0;
            }

            if(material.occlusionTexture.index != -1) {
                u32 texture_index = material.occlusionTexture.index;
                u32 image_index = model.textures[texture_index].source;
                material_info.occlusion_map = images[image_index]->get_texture_id();
                material_info.has_occlusion_map = 1;
            } else {
                material_info.occlusion_map = default_texture->get_texture_id();
                material_info.has_occlusion_map = 0;
            }

            if(material.emissiveTexture.index != -1) {
                u32 texture_index = material.emissiveTexture.index;
                u32 image_index = model.textures[texture_index].source;
                material_info.emissive_map = images[image_index]->get_texture_id();
                material_info.has_emissive_map = 1;
                material_info.emissive_factor = { 1.0f, 1.0f, 1.0f };
            } else {
                std::vector<f64>& vector = material.emissiveFactor;
                material_info.emissive_factor = { static_cast<f32>(vector[0]), static_cast<f32>(vector[1]), static_cast<f32>(vector[2]) };
                material_info.emissive_map = default_texture->get_texture_id();
                material_info.has_emissive_map = 0;
            }

            material_infos.push_back(std::move(material_info));
        }

        for(usize i = 0; i < material_infos.size(); i++) {
            daxa::BufferId material_staging_buffer = device.create_buffer({
                .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .size = sizeof(MaterialInfo),
            });

            daxa::BufferId material_info_buffer = device.create_buffer({
                .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY,
                .size = sizeof(MaterialInfo),
            });

            auto cmd_list = device.create_command_list({
                .debug_name = APPNAME_PREFIX("cmd_list"),
            });

            auto buffer_ptr = device.map_memory_as<MaterialInfo>(material_staging_buffer);
            std::memcpy(buffer_ptr, &material_infos[i], sizeof(MaterialInfo));
            device.unmap_memory(material_staging_buffer);

            cmd_list.pipeline_barrier({
                .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
                .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_READ,
            });

            cmd_list.copy_buffer_to_buffer({
                .src_buffer = material_staging_buffer,
                .dst_buffer = material_info_buffer,
                .size = static_cast<u32>(sizeof(MaterialInfo)),
            });

            cmd_list.complete();
            device.submit_commands({
                .command_lists = {std::move(cmd_list)}
            });

            device.wait_idle();
            device.destroy_buffer(material_staging_buffer);

            material_buffers.push_back(material_info_buffer);
            material_buffer_addresses.push_back(device.buffer_reference(material_info_buffer));
        }

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

                    Primitive temp_primitive {
                        .first_index = indexOffset,
                        .first_vertex = vertexOffset,
                        .index_count = indexCount,
                        .vertex_count = vertexCount,
                        .material_index = static_cast<u32>(primitive.material) 
                    };

                    primitives.push_back(temp_primitive);

                    vertexOffset += vertexCount;
                    indexOffset += indexCount;
                }
            }
        }

        vertex_buffer = device.create_buffer(daxa::BufferInfo{
            .size = static_cast<u32>(sizeof(DrawVertex) * vertices.size()),
            .debug_name = APPNAME_PREFIX("vertex_buffer"),
        });

        index_buffer = device.create_buffer(daxa::BufferInfo{
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
        vertex_buffer_address = device.buffer_reference(vertex_buffer);
    }

    Model::~Model() {
        device.destroy_buffer(vertex_buffer);
        device.destroy_buffer(index_buffer);
        for (auto & buffer : material_buffers) {
            device.destroy_buffer(buffer);
        }
    }

    void Model::bind_index_buffer(daxa::CommandList & cmd_list) {
        cmd_list.set_index_buffer(index_buffer, 0, 4);
    }

    void Model::draw(daxa::CommandList & cmd_list) {
        for (auto & primitive : primitives) {
            if (primitive.index_count > 0) {
                cmd_list.draw_indexed({
                    .index_count = primitive.index_count,
                    .instance_count = 1,
                    .first_index = primitive.first_index,
                    .vertex_offset = static_cast<i32>(primitive.first_vertex),
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

    void Model::draw(daxa::CommandList & cmd_list, DrawPush& push_constant) {
        for (auto & primitive : primitives) {
            push_constant.face_buffer = vertex_buffer_address;
            push_constant.material_info = material_buffer_addresses[primitive.material_index];
            cmd_list.push_constant(push_constant);

            if (primitive.index_count > 0) {
                cmd_list.draw_indexed({
                    .index_count = primitive.index_count,
                    .instance_count = 1,
                    .first_index = primitive.first_index,
                    .vertex_offset = static_cast<i32>(primitive.first_vertex),
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
}