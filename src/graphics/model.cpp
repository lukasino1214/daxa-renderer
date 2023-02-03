#include "model.hpp"
#include <daxa/command_list.hpp>


#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <iostream>

#include <glm/gtc/type_ptr.hpp>

#include <cassert>
#include <fastgltf_types.hpp>
#include <fastgltf_parser.hpp>

#include "../utils/threadpool.hpp"

namespace dare {
    Model::Model(daxa::Device device, const std::string& path) : device{device}, path{path} {
        std::cout << "Loading model: " << path << std::endl;
        auto timer = std::chrono::system_clock::now();
        std::vector<DrawVertex> vertices{};
        std::vector<u32> indices{};

        std::unique_ptr<fastgltf::glTF> gltf;
        fastgltf::GltfDataBuffer data_buffer;
        std::unique_ptr<fastgltf::Asset> asset;

        {
            fastgltf::Parser parser(fastgltf::Extensions::KHR_mesh_quantization);

            auto _path = std::filesystem::path{path};

            constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;
            data_buffer.loadFromFile(_path);

            if (_path.extension() == ".gltf") {
                gltf = parser.loadGLTF(&data_buffer, _path.parent_path(), gltfOptions);
            } else if (_path.extension() == ".glb") {
                gltf = parser.loadBinaryGLTF(&data_buffer, _path.parent_path(), gltfOptions);
            }

            if (parser.getError() != fastgltf::Error::None) {
                std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(parser.getError()) << std::endl;
                
            }

            auto error = gltf->parse(fastgltf::Category::Scenes);
            if (error != fastgltf::Error::None) {
                std::cerr << "Failed to parse glTF: " << fastgltf::to_underlying(error) << std::endl;
                
            }

            asset = gltf->getParsedAsset();
        }

        auto get_image_type = [&](usize image_index) -> Texture::Type {
            for(auto& material : asset->materials) {
                if(material.pbrData.value().baseColorTexture.has_value()) {
                    u32 diffuseTextureIndex = material.pbrData.value().baseColorTexture.value().textureIndex;
                    auto& diffuseTexture = asset->textures[diffuseTextureIndex];
                    if (image_index == diffuseTexture.imageIndex.value()) {
                        return Texture::Type::SRGB;
                    }
                }
            }

            return Texture::Type::UNORM;
        };

        images.resize(asset->images.size());
        std::vector<std::pair<std::pair<std::unique_ptr<Texture>, daxa::CommandList>, u32>> textures;
        textures.resize(asset->images.size());
        ThreadPool pool(std::thread::hardware_concurrency());

        auto process_image = [&](fastgltf::Image& image, u32 index) {
            std::pair<std::unique_ptr<Texture>, daxa::CommandList> tex;
            switch (image.location) {
                case fastgltf::DataLocation::FilePathWithByteRange: {
                    auto path = image.data.path.string();
                    //tex = std::make_unique<Texture>(device, path);
                    break;
                }
                case fastgltf::DataLocation::VectorWithMime: {
                    int width = 0, height = 0, nrChannels = 0;
                    unsigned char *data = nullptr;
                    data = stbi_load_from_memory(image.data.bytes.data(), static_cast<int>(image.data.bytes.size()), &width, &height, &nrChannels, 0);
                    if(!data) {
                        throw std::runtime_error("wtf");
                    }

                    // utter garbage thanks to RGB formats are no supported
                    unsigned char* buffer = nullptr;
                    u64 buffer_size;

                    if (nrChannels == 3) {
                        buffer_size = width * height * 4;
                        std::vector<unsigned char> image_data(buffer_size, 255);

                        buffer = (unsigned char*)image_data.data();
                        unsigned char* rgba = buffer;
                        unsigned char* rgb = data;
                        for (usize i = 0; i < width * height; ++i) {
                            std::memcpy(rgba, rgb, sizeof(unsigned char) * 3);
                            rgba += 4;
                            rgb += 3;
                        }

                        //tex = std::make_unique<Texture>(device, width, height, buffer, get_image_type(index));
                        tex = Texture::load_texture(device, width, height, buffer, get_image_type(index));
                    }
                    else {
                        buffer = data;
                        buffer_size = width * height * 4;
                        //tex = std::make_unique<Texture>(device, width, height, buffer, get_image_type(index));
                        tex = Texture::load_texture(device, width, height, buffer, get_image_type(index));
                    }

                    stbi_image_free(data);
                    break;
                }
                case fastgltf::DataLocation::BufferViewWithMime: {
                    auto& bufferView = asset->bufferViews[image.data.bufferViewIndex];
                    auto& buffer = asset->buffers[bufferView.bufferIndex];
                    switch (buffer.location) {
                        case fastgltf::DataLocation::VectorWithMime: {
                            int width = 0, height = 0, nrChannels = 0;
                            unsigned char *data = nullptr;
                            data = stbi_load_from_memory(image.data.bytes.data(), static_cast<int>(image.data.bytes.size()), &width, &height, &nrChannels, 0);
                            if(!data) {
                                throw std::runtime_error("wtf");
                            }

                            // utter garbage thanks to RGB formats are no supported
                            unsigned char* buffer;
                            u64 buffer_size;

                            if (nrChannels == 3) {
                                buffer_size = width * height * 4;
                                std::vector<unsigned char> image_data(buffer_size, 255);

                                buffer = (unsigned char*)image_data.data();
                                unsigned char* rgba = buffer;
                                unsigned char* rgb = data;
                                for (usize i = 0; i < width * height; ++i) {
                                    std::memcpy(rgba, rgb, sizeof(unsigned char) * 3);
                                    rgba += 4;
                                    rgb += 3;
                                }
                            }
                            else {
                                buffer = data;
                                buffer_size = width * height * 4;
                            }

                            tex = Texture::load_texture(device, width, height, buffer, get_image_type(index));
                            //tex = std::make_unique<Texture>(device, width, height, buffer, get_image_type(index));
                            stbi_image_free(data);
                            break;
                        }
                    }
                    break;
                }
                case fastgltf::DataLocation::None: {
                    break;
                }
            }

            textures[index] = std::pair{std::move(tex), index};
        };

        auto texture_timer = std::chrono::system_clock::now();
        for (u32 i = 0; i < asset->images.size(); i++) {
            auto& image = asset->images[i];
            pool.push_task(process_image, image, i);
        }

        pool.wait_for_tasks();

        for(auto& tex : textures) {
            device.submit_commands({
                .command_lists = {std::move(tex.first.second)},
            });

            images[tex.second] = std::move(tex.first.first);
        }

        device.wait_idle();

        std::cout << "- textures loaded in " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - texture_timer).count() << " ms!" << std::endl;


        default_texture = std::make_unique<Texture>(device, "assets/textures/white.png");

        auto material_timer = std::chrono::system_clock::now();
        for (auto& material : asset->materials) {
            MaterialInfo material_info {};

            if(material.pbrData.value().baseColorTexture.has_value()) {
                u32 texture_index = material.pbrData.value().baseColorTexture.value().textureIndex;
                u32 image_index = asset->textures[texture_index].imageIndex.value();
                material_info.albedo = images[image_index]->get_texture_id();
                material_info.albedo_factor = { 1.0f, 1.0f, 1.0f, 1.0f };
                material_info.has_albedo = 1;
            } else {
                material_info.albedo = default_texture->get_texture_id();
                auto& vector = material.pbrData.value().baseColorFactor;
                material_info.albedo_factor = { static_cast<f32>(vector[0]), static_cast<f32>(vector[1]), static_cast<f32>(vector[2]), static_cast<f32>(vector[3]) };
                material_info.has_albedo = 0;
            }

            if(material.pbrData.value().metallicRoughnessTexture.has_value()) {
                u32 texture_index = material.pbrData.value().metallicRoughnessTexture.value().textureIndex;
                u32 image_index = asset->textures[texture_index].imageIndex.value();
                material_info.metallic_roughness = images[image_index]->get_texture_id();
                material_info.has_metallic_roughness = 1;
                material_info.metallic = 1.0f;
                material_info.roughness = 1.0f;
            } else {
                material_info.metallic_roughness = default_texture->get_texture_id();
                material_info.has_metallic_roughness = 0;
                material_info.metallic = static_cast<f32>(material.pbrData.value().metallicFactor);
                material_info.roughness = static_cast<f32>(material.pbrData.value().roughnessFactor);
            }

            if(material.normalTexture.has_value()) {
                u32 texture_index = material.normalTexture.value().textureIndex;
                u32 image_index = asset->textures[texture_index].imageIndex.value();
                material_info.normal_map = images[image_index]->get_texture_id();
                material_info.has_normal_map = 1;
            } else {
                material_info.normal_map = default_texture->get_texture_id();
                material_info.has_normal_map = 0;
            }

            if(material.occlusionTexture.has_value()) {
                u32 texture_index = material.occlusionTexture.value().textureIndex;
                u32 image_index = asset->textures[texture_index].imageIndex.value();
                material_info.occlusion_map = images[image_index]->get_texture_id();
                material_info.has_occlusion_map = 1;
            } else {
                material_info.occlusion_map = default_texture->get_texture_id();
                material_info.has_occlusion_map = 0;
            }

            if(material.emissiveTexture.has_value()) {
                u32 texture_index = material.emissiveTexture.value().textureIndex;
                u32 image_index = asset->textures[texture_index].imageIndex.value();
                material_info.emissive_map = images[image_index]->get_texture_id();
                material_info.has_emissive_map = 1;
                material_info.emissive_factor = { 1.0f, 1.0f, 1.0f };
            } else {
                auto& vector = material.emissiveFactor;
                material_info.emissive_factor = { static_cast<f32>(vector[0]), static_cast<f32>(vector[1]), static_cast<f32>(vector[2]) };
                material_info.emissive_map = default_texture->get_texture_id();
                material_info.has_emissive_map = 0;
            }

            daxa::BufferId material_staging_buffer = device.create_buffer({
                .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .size = sizeof(MaterialInfo),
            });

            daxa::BufferId material_info_buffer = device.create_buffer({
                .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY,
                .size = sizeof(MaterialInfo),
            });

            auto cmd_list = device.create_command_list({
                .debug_name = "cmd_list",
            });

            auto buffer_ptr = device.get_host_address_as<MaterialInfo>(material_staging_buffer);
            std::memcpy(buffer_ptr, &material_info, sizeof(MaterialInfo));

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
            material_buffer_addresses.push_back(device.get_device_address(material_info_buffer));

            material_infos.push_back(std::move(material_info));
        }

        std::cout << "- materials loaded in " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - material_timer).count() << " ms!" << std::endl;

        auto vertices_timer = std::chrono::system_clock::now();
        for(auto& scene : asset->scenes) {
            for (usize i = 0; i < scene.nodeIndices.size(); i++) {
                auto & node = asset->nodes[i];
                uint32_t vertexOffset = 0;
                uint32_t indexOffset = 0;

                for (auto& primitive : asset->meshes[node.meshIndex.value()].primitives) {
                    uint32_t vertexCount = 0;
                    uint32_t indexCount = 0;

                    const float* positionBuffer = nullptr;
                    const float* normalBuffer = nullptr;
                    const float* texCoordsBuffer = nullptr;
                    const float* tangentsBuffer = nullptr;

                    if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
                        auto& accessor = asset->accessors[primitive.attributes.find("POSITION")->second];
                        auto& view = asset->bufferViews[accessor.bufferViewIndex.value()];
                        positionBuffer = reinterpret_cast<const float*>(&(asset->buffers[view.bufferIndex].data.bytes[accessor.byteOffset + view.byteOffset]));
                        vertexCount = accessor.count;
                    }

                    if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                        auto& accessor = asset->accessors[primitive.attributes.find("NORMAL")->second];
                        auto& view = asset->bufferViews[accessor.bufferViewIndex.value()];
                        normalBuffer = reinterpret_cast<const float*>(&(asset->buffers[view.bufferIndex].data.bytes[accessor.byteOffset + view.byteOffset]));
                    }

                    if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                        auto& accessor = asset->accessors[primitive.attributes.find("TEXCOORD_0")->second];
                        auto& view = asset->bufferViews[accessor.bufferViewIndex.value()];
                        texCoordsBuffer = reinterpret_cast<const float*>(&(asset->buffers[view.bufferIndex].data.bytes[accessor.byteOffset + view.byteOffset]));
                    }

                    if (primitive.attributes.find("TANGENT") != primitive.attributes.end()) {
                        auto& accessor = asset->accessors[primitive.attributes.find("TANGENT")->second];
                        auto& view = asset->bufferViews[accessor.bufferViewIndex.value()];
                        tangentsBuffer = reinterpret_cast<const float*>(&(asset->buffers[view.bufferIndex].data.bytes[accessor.byteOffset + view.byteOffset]));
                    }

                    vertices.reserve((vertices.size() + vertexCount) * sizeof(DrawVertex));
                    for (size_t v = 0; v < vertexCount; v++) {
                        glm::vec3 temp_position = glm::make_vec3(&positionBuffer[v * 3]);
                        glm::vec3 temp_normal = glm::make_vec3(&normalBuffer[v * 3]);
                        glm::vec2 temp_uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec2(0.0f);
                        glm::vec4 temp_tangent = tangentsBuffer ? glm::make_vec4(&tangentsBuffer[v * 4]) : glm::vec4(0.0);
                        DrawVertex vertex{
                            .position = {temp_position.x, temp_position.y, temp_position.z},
                            .normal = {temp_normal.x, temp_normal.y, temp_normal.z},
                            .uv = {temp_uv.x, temp_uv.y},
                            .tangent = {temp_tangent.x, temp_tangent.y, temp_tangent.z, temp_tangent.w}
                        };

                        vertices.emplace_back(std::move(vertex));
                    }

                    {
                        auto& accessor = asset->accessors[primitive.indicesAccessor.value()];
                        auto& bufferView = asset->bufferViews[accessor.bufferViewIndex.value()];
                        auto& buffer = asset->buffers[bufferView.bufferIndex];

                        indexCount += static_cast<uint32_t>(accessor.count);

                        switch(accessor.componentType) {
                            case fastgltf::ComponentType::UnsignedInt: {
                                const uint32_t * buf = reinterpret_cast<const uint32_t *>(&buffer.data.bytes[accessor.byteOffset + bufferView.byteOffset]);
                                indices.reserve((indices.size() + accessor.count) * sizeof(uint32_t));
                                for (size_t index = 0; index < accessor.count; index++) {
                                    indices.push_back(buf[index]);
                                }
                                break;
                            }
                            case fastgltf::ComponentType::UnsignedShort: {
                                const uint16_t * buf = reinterpret_cast<const uint16_t *>(&buffer.data.bytes[accessor.byteOffset + bufferView.byteOffset]);
                                indices.reserve((indices.size() + accessor.count) * sizeof(uint16_t));
                                for (size_t index = 0; index < accessor.count; index++) {
                                    indices.push_back(buf[index]);
                                }
                                break;
                            }
                            case fastgltf::ComponentType::UnsignedByte: {
                                const uint8_t * buf = reinterpret_cast<const uint8_t *>(&buffer.data.bytes[accessor.byteOffset + bufferView.byteOffset]);
                                indices.reserve((indices.size() + accessor.count) * sizeof(uint8_t));
                                for (size_t index = 0; index < accessor.count; index++) {
                                    indices.push_back(buf[index]);
                                }
                                break;
                            }
                        }
                    }

                    Primitive temp_primitive {
                        .first_index = indexOffset,
                        .first_vertex = vertexOffset,
                        .index_count = indexCount,
                        .vertex_count = vertexCount,
                        .material_index = static_cast<u32>(primitive.materialIndex.value()) 
                    };

                    primitives.push_back(temp_primitive);

                    vertexOffset += vertexCount;
                    indexOffset += indexCount;
                }
            }
        }

        vertex_buffer = device.create_buffer(daxa::BufferInfo{
            .size = static_cast<u32>(sizeof(DrawVertex) * vertices.size()),
            .debug_name = "vertex_buffer",
        });

        auto vertex_staging_buffer = device.create_buffer({
            .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            .size = static_cast<u32>(sizeof(DrawVertex) * vertices.size()),
            .debug_name = "vertex_staging_buffer",
        });

        index_buffer = device.create_buffer(daxa::BufferInfo{
            .size = static_cast<u32>(sizeof(u32) * indices.size()),
            .debug_name = "index_buffer",
        });

        auto index_staging_buffer = device.create_buffer({
            .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            .size = static_cast<u32>(sizeof(u32) * indices.size()),
            .debug_name = "index_staging_buffer",
        });

        {
            auto buffer_ptr = device.get_host_address_as<DrawVertex>(vertex_staging_buffer);
            std::memcpy(buffer_ptr, vertices.data(), vertices.size() * sizeof(DrawVertex));
        }

        {
            auto buffer_ptr = device.get_host_address_as<u32>(index_staging_buffer);
            std::memcpy(buffer_ptr, indices.data(), indices.size() * sizeof(u32));
        }

        auto cmd_list = device.create_command_list({
            .debug_name = "cmd_list",
        });

        cmd_list.copy_buffer_to_buffer({
            .src_buffer = vertex_staging_buffer,
            .dst_buffer = vertex_buffer,
            .size = static_cast<u32>(sizeof(DrawVertex) * vertices.size()),
        });

        cmd_list.copy_buffer_to_buffer({
            .src_buffer = index_staging_buffer,
            .dst_buffer = index_buffer,
            .size = static_cast<u32>(sizeof(u32) * indices.size()),
        });

        cmd_list.destroy_buffer_deferred(vertex_staging_buffer);
        cmd_list.destroy_buffer_deferred(index_staging_buffer);
        cmd_list.complete();
        device.submit_commands({
            .command_lists = {std::move(cmd_list)},
        });

        std::cout << "- vertex and index buffer loaded in " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - vertices_timer).count() << " ms!" << std::endl;

        std::cout << "loaded in " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - timer).count() << " ms!" << std::endl;
        vertex_buffer_address = device.get_device_address(vertex_buffer);
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
        cmd_list.set_index_buffer(this->index_buffer, 0, 4);
        for (auto & primitive : primitives) {
            push_constant.face_buffer = vertex_buffer_address;
            push_constant.material_info_buffer = material_buffer_addresses[primitive.material_index];
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