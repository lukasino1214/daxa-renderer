#include "new_model.hpp"

#include <stb_image.h>
#include <cstring>
#include <iostream>

#include <cassert>
#include <fastgltf_types.hpp>
#include <fastgltf_parser.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace dare {
    NewModel::NewModel(daxa::Device device, const std::string& _path) : device{device} {
        std::cout << "Loading " << _path << std::endl;
        auto timer = std::chrono::system_clock::now();

        std::unique_ptr<fastgltf::glTF> gltf;
        fastgltf::GltfDataBuffer data;
        std::unique_ptr<fastgltf::Asset> asset;

        // Parse the glTF file and get the constructed asset
        {
            fastgltf::Parser parser(fastgltf::Extensions::KHR_mesh_quantization);

            auto path = std::filesystem::path{_path};

            constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;
            data.loadFromFile(path);

            if (path.extension() == ".gltf") {
                gltf = parser.loadGLTF(&data, path.parent_path(), gltfOptions);
            } else if (path.extension() == ".glb") {
                gltf = parser.loadBinaryGLTF(&data, path.parent_path(), gltfOptions);
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

        default_texture = std::make_unique<Texture>(device, "assets/textures/white.png");

        std::vector<DrawVertex> vertices{};
        std::vector<u32> indices{};

        for (auto& image : asset->images) {
            switch (image.location) {
                case fastgltf::DataLocation::FilePathWithByteRange: {
                    auto path = image.data.path.string();
                    std::unique_ptr<Texture> tex = std::make_unique<Texture>(device, path);
                    images.push_back(std::move(tex));
                    std::cout << path << std::endl;
                    std::cout << "Image: FilePathWithByteRange" << std::endl;
                    break;
                }
                case fastgltf::DataLocation::VectorWithMime: {
                    int width, height, nrChannels;
                    unsigned char *data = stbi_load_from_memory(image.data.bytes.data(), static_cast<int>(image.data.bytes.size()), &width, &height, &nrChannels, 0);
                    
                    // utter garbage thanks to RGB formats are no supported
                    unsigned char* buffer;
                    u64 buffer_size = width * height * 4;
                    std::vector<unsigned char> image_data(buffer_size, 0x00);
                    buffer = (unsigned char*)image_data.data();
                    unsigned char* rgba = buffer;
                    unsigned char* rgb = data;
                    for (usize i = 0; i < width * height; ++i) {
                        std::memcpy(rgba, rgb, sizeof(unsigned char) * 3);
                        rgba += 4;
                        rgb += 3;
                    }

                    std::unique_ptr<Texture> tex = std::make_unique<Texture>(device, width, height, buffer, Texture::Type::SRGB);
                    images.push_back(std::move(tex));
                    stbi_image_free(data);
                    std::cout << "Image: VectorWithMime" << std::endl;
                    break;
                }
                case fastgltf::DataLocation::BufferViewWithMime: {
                    auto& bufferView = asset->bufferViews[image.data.bufferViewIndex];
                    auto& buffer = asset->buffers[bufferView.bufferIndex];
                    switch (buffer.location) {
                        case fastgltf::DataLocation::VectorWithMime: {
                            int width, height, nrChannels;
                            unsigned char *data = stbi_load_from_memory(buffer.data.bytes.data() + bufferView.byteOffset, static_cast<int>(bufferView.byteLength), &width, &height, &nrChannels, 0);
                            
                            // utter garbage thanks to RGB formats are no supported
                            unsigned char* buffer;
                            u64 buffer_size = width * height * 4;
                            std::vector<unsigned char> image_data(buffer_size, 0x00);
                            buffer = (unsigned char*)image_data.data();
                            unsigned char* rgba = buffer;
                            unsigned char* rgb = data;
                            for (usize i = 0; i < width * height; ++i) {
                                std::memcpy(rgba, rgb, sizeof(unsigned char) * 3);
                                rgba += 4;
                                rgb += 3;
                            }

                            std::unique_ptr<Texture> tex = std::make_unique<Texture>(device, width, height, buffer, Texture::Type::SRGB);
                            images.push_back(std::move(tex));
                            stbi_image_free(data);
                            break;
                        }
                    }
                    std::cout << "Image: BufferViewWithMime" << std::endl;
                    break;
                }
                case fastgltf::DataLocation::None: {
                    break;
                }
            }
        }
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
        /*for (auto& mesh : asset->meshes) {
            
        }*/

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
                        /*const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.find("POSITION")->second];
                        const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                        positionBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                        vertexCount = accessor.count;*/

                        auto& accessor = asset->accessors[primitive.attributes.find("POSITION")->second];
                        auto& view = asset->bufferViews[accessor.bufferViewIndex.value()];
                        positionBuffer = reinterpret_cast<const float*>(&(asset->buffers[view.bufferIndex].data.bytes[accessor.byteOffset + view.byteOffset]));
                        vertexCount = accessor.count;
                    }

                    if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                        /*const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.find("NORMAL")->second];
                        const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                        normalBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));*/


                        auto& accessor = asset->accessors[primitive.attributes.find("NORMAL")->second];
                        auto& view = asset->bufferViews[accessor.bufferViewIndex.value()];
                        normalBuffer = reinterpret_cast<const float*>(&(asset->buffers[view.bufferIndex].data.bytes[accessor.byteOffset + view.byteOffset]));
                    }

                    if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                        /*const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
                        const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                        texCoordsBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));*/
                        auto& accessor = asset->accessors[primitive.attributes.find("TEXCOORD_0")->second];
                        auto& view = asset->bufferViews[accessor.bufferViewIndex.value()];
                        texCoordsBuffer = reinterpret_cast<const float*>(&(asset->buffers[view.bufferIndex].data.bytes[accessor.byteOffset + view.byteOffset]));
                    }

                    if (primitive.attributes.find("TANGENT") != primitive.attributes.end()) {
                        /*const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.find("TANGENT")->second];
                        const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                        tangentsBuffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));*/
                        auto& accessor = asset->accessors[primitive.attributes.find("TANGENT")->second];
                        auto& view = asset->bufferViews[accessor.bufferViewIndex.value()];
                        tangentsBuffer = reinterpret_cast<const float*>(&(asset->buffers[view.bufferIndex].data.bytes[accessor.byteOffset + view.byteOffset]));
                    }

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

                        vertices.push_back(vertex);
                    }

                    {
                        auto& accessor = asset->accessors[primitive.indicesAccessor.value()];
                        auto& bufferView = asset->bufferViews[accessor.bufferViewIndex.value()];
                        auto& buffer = asset->buffers[bufferView.bufferIndex];

                        indexCount += static_cast<uint32_t>(accessor.count);

                        switch(accessor.componentType) {
                            case fastgltf::ComponentType::UnsignedInt: {
                                const uint32_t * buf = reinterpret_cast<const uint32_t *>(&buffer.data.bytes[accessor.byteOffset + bufferView.byteOffset]);
                                for (size_t index = 0; index < accessor.count; index++) {
                                    indices.push_back(buf[index]);
                                    std::cout << buf[index] << std::endl;
                                }
                                break;
                            }
                            case fastgltf::ComponentType::UnsignedShort: {
                                const uint16_t * buf = reinterpret_cast<const uint16_t *>(&buffer.data.bytes[accessor.byteOffset + bufferView.byteOffset]);
                                for (size_t index = 0; index < accessor.count; index++) {
                                    indices.push_back(buf[index]);
                                    std::cout << buf[index] << std::endl;
                                }
                                break;
                            }
                            case fastgltf::ComponentType::UnsignedByte: {
                                const uint8_t * buf = reinterpret_cast<const uint8_t *>(&buffer.data.bytes[accessor.byteOffset + bufferView.byteOffset]);
                                for (size_t index = 0; index < accessor.count; index++) {
                                    indices.push_back(buf[index]);
                                    std::cout << buf[index] << std::endl;
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

        index_buffer = device.create_buffer(daxa::BufferInfo{
            .size = static_cast<u32>(sizeof(u32) * indices.size()),
            .debug_name = "index_buffer",
        });

        {
            auto cmd_list = device.create_command_list({
                .debug_name = "cmd_list",
            });

            auto vertex_staging_buffer = device.create_buffer({
                .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .size = static_cast<u32>(sizeof(DrawVertex) * vertices.size()),
                .debug_name = "vertex_staging_buffer",
            });
            cmd_list.destroy_buffer_deferred(vertex_staging_buffer);

            auto buffer_ptr = device.get_host_address_as<DrawVertex>(vertex_staging_buffer);
            std::memcpy(buffer_ptr, vertices.data(), vertices.size() * sizeof(DrawVertex));

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
                .debug_name = "cmd_list",
            });

            auto index_staging_buffer = device.create_buffer({
                .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .size = static_cast<u32>(sizeof(u32) * indices.size()),
                .debug_name = "index_staging_buffer",
            });
            cmd_list.destroy_buffer_deferred(index_staging_buffer);

            auto buffer_ptr = device.get_host_address_as<u32>(index_staging_buffer);
            std::memcpy(buffer_ptr, indices.data(), indices.size() * sizeof(u32));

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

        std::cout << _path << " loaded in " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - timer).count() << " ms!" << std::endl;
        vertex_buffer_address = device.get_device_address(vertex_buffer);
    }

    NewModel::~NewModel() {
        device.destroy_buffer(vertex_buffer);
        device.destroy_buffer(index_buffer);
        for (auto & buffer : material_buffers) {
            device.destroy_buffer(buffer);
        }
    }

}

/*void NewModel::load_image(const std::unique_ptr<fastgltf::Asset>& asset, fastgltf::Image& image) {
    switch (image.location) {
        case fastgltf::DataLocation::FilePathWithByteRange: {
            auto path = image.data.path.string();
            std::unique_ptr<Texture> tex = std::make_unique<Texture>(device, path);
            images.push_back(std::move(tex));
            std::cout << path << std::endl;
            std::cout << "Image: FilePathWithByteRange" << std::endl;
            break;
        }
        case fastgltf::DataLocation::VectorWithMime: {
            int width, height, nrChannels;
            unsigned char *data = stbi_load_from_memory(image.data.bytes.data(), static_cast<int>(image.data.bytes.size()), &width, &height, &nrChannels, 0);
            
            // utter garbage thanks to RGB formats are no supported
            unsigned char* buffer;
            u64 buffer_size = width * height * 4;
            std::vector<unsigned char> image_data(buffer_size, 0x00);
            buffer = (unsigned char*)image_data.data();
            unsigned char* rgba = buffer;
            unsigned char* rgb = data;
            for (usize i = 0; i < width * height; ++i) {
                std::memcpy(rgba, rgb, sizeof(unsigned char) * 3);
                rgba += 4;
                rgb += 3;
            }

            std::unique_ptr<Texture> tex = std::make_unique<Texture>(device, width, height, buffer, Texture::TextureType::SRGB);
            images.push_back(std::move(tex));
            stbi_image_free(data);
            std::cout << "Image: VectorWithMime" << std::endl;
            break;
        }
        case fastgltf::DataLocation::BufferViewWithMime: {
            auto& bufferView = asset->bufferViews[image.data.bufferViewIndex];
            auto& buffer = asset->buffers[bufferView.bufferIndex];
            switch (buffer.location) {
                case fastgltf::DataLocation::VectorWithMime: {
                    int width, height, nrChannels;
                    unsigned char *data = stbi_load_from_memory(buffer.data.bytes.data() + bufferView.byteOffset, static_cast<int>(bufferView.byteLength), &width, &height, &nrChannels, 0);
                    
                    // utter garbage thanks to RGB formats are no supported
                    unsigned char* buffer;
                    u64 buffer_size = width * height * 4;
                    std::vector<unsigned char> image_data(buffer_size, 0x00);
                    buffer = (unsigned char*)image_data.data();
                    unsigned char* rgba = buffer;
                    unsigned char* rgb = data;
                    for (usize i = 0; i < width * height; ++i) {
                        std::memcpy(rgba, rgb, sizeof(unsigned char) * 3);
                        rgba += 4;
                        rgb += 3;
                    }

                    std::unique_ptr<Texture> tex = std::make_unique<Texture>(device, width, height, buffer, Texture::TextureType::SRGB);
                    images.push_back(std::move(tex));
                    stbi_image_free(data);
                    break;
                }
            }
            std::cout << "Image: BufferViewWithMime" << std::endl;
            break;
        }
        case fastgltf::DataLocation::None: {
            break;
        }
    }
}

void NewModel::load_material(const std::unique_ptr<fastgltf::Asset>& asset, fastgltf::Material& material) {

}

void NewModel::load_mesh(const std::unique_ptr<fastgltf::Asset>& asset, fastgltf::Mesh& mesh) {
    u32 vertex_count = 0;
    u32 index_cout = 0;
    for (auto it = mesh.primitives.begin(); it != mesh.primitives.end(); ++it) {
        auto& indices = asset->accessors[it->indicesAccessor.value()];
        if (!indices.bufferViewIndex.has_value())
            return;
        std::cout << static_cast<uint32_t>(indices.count) << std::endl;
        //auto& indicesView = asset->bufferViews[indices.bufferViewIndex.value()];
        //draw.firstIndex = static_cast<uint32_t>(indices.byteOffset + indicesView.byteOffset) / fastgltf::getElementByteSize(indices.type, indices.componentType);
    }
    /*Mesh out_mesh = {};
    out_mesh.primitives.resize(mesh.primitives.size());
    for (auto it = mesh.primitives.begin(); it != mesh.primitives.end(); ++it) {
        if (it->attributes.find("POSITION") == it->attributes.end())
            continue;

        if (!it->indicesAccessor.has_value()) {
            return;
        }

        // Generate the VAO

        // Get the output primitive
        auto index = std::distance(mesh.primitives.begin(), it);
        auto& primitive = out_mesh.primitives[index];
        if (it->materialIndex.has_value()) {
            primitive.material_index = it->materialIndex.value();
        }

        {
            // Position
            auto& positionAccessor = asset->accessors[it->attributes["POSITION"]];
            if (!positionAccessor.bufferViewIndex.has_value())
                continue;

            glEnableVertexArrayAttrib(vao, 0);
            glVertexArrayAttribFormat(vao, 0,
                                      static_cast<GLint>(fastgltf::getNumComponents(positionAccessor.type)),
                                      fastgltf::getGLComponentType(positionAccessor.componentType),
                                      GL_FALSE, 0);
            glVertexArrayAttribBinding(vao, 0, 0);

            auto& positionView = asset->bufferViews[positionAccessor.bufferViewIndex.value()];
            auto offset = positionView.byteOffset + positionAccessor.byteOffset;
            if (positionView.byteStride.has_value()) {
                glVertexArrayVertexBuffer(vao, 0, viewer->buffers[positionView.bufferIndex],
                                          static_cast<GLintptr>(offset),
                                          static_cast<GLsizei>(positionView.byteStride.value()));
            } else {
                glVertexArrayVertexBuffer(vao, 0, viewer->buffers[positionView.bufferIndex],
                                          static_cast<GLintptr>(offset),
                                          static_cast<GLsizei>(fastgltf::getElementByteSize(positionAccessor.type, positionAccessor.componentType)));
            }
        }

        {
            // Tex coord
            auto& texCoordAccessor = asset->accessors[it->attributes["TEXCOORD_0"]];
            if (!texCoordAccessor.bufferViewIndex.has_value())
                continue;

            glEnableVertexArrayAttrib(vao, 1);
            glVertexArrayAttribFormat(vao, 1, static_cast<GLint>(fastgltf::getNumComponents(texCoordAccessor.type)),
                                      fastgltf::getGLComponentType(texCoordAccessor.componentType),
                                      GL_FALSE, 0);
            glVertexArrayAttribBinding(vao, 1, 1);

            auto& texCoordView = asset->bufferViews[texCoordAccessor.bufferViewIndex.value()];
            auto offset = texCoordView.byteOffset + texCoordAccessor.byteOffset;
            if (texCoordView.byteStride.has_value()) {
                glVertexArrayVertexBuffer(vao, 1, viewer->buffers[texCoordView.bufferIndex],
                                          static_cast<GLintptr>(offset),
                                          static_cast<GLsizei>(texCoordView.byteStride.value()));
            } else {
                glVertexArrayVertexBuffer(vao, 1, viewer->buffers[texCoordView.bufferIndex],
                                          static_cast<GLintptr>(offset),
                                          static_cast<GLsizei>(fastgltf::getElementByteSize(texCoordAccessor.type, texCoordAccessor.componentType)));
            }
        }

        // Generate the indirect draw command
        auto& draw = primitive.draw;
        draw.instanceCount = 1;
        draw.baseInstance = 0;
        draw.baseVertex = 0;

        auto& indices = asset->accessors[it->indicesAccessor.value()];
        if (!indices.bufferViewIndex.has_value())
            return false;
        draw.count = static_cast<uint32_t>(indices.count);

        auto& indicesView = asset->bufferViews[indices.bufferViewIndex.value()];
        draw.firstIndex = static_cast<uint32_t>(indices.byteOffset + indicesView.byteOffset) / fastgltf::getElementByteSize(indices.type, indices.componentType);
        primitive.indexType = getGLComponentType(indices.componentType);
        glVertexArrayElementBuffer(vao, viewer->buffers[indicesView.bufferIndex]);
    }


    meshes.emplace_back(out_mesh);*/
//}

