#include "new_model.hpp"

#include <stb_image.h>
#include <cstring>

#include <fastgltf_parser.hpp>


NewModel::NewModel(daxa::Device device, const std::string& _path) : device{device} {
    std::cout << "Loading " << _path << std::endl;

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

    for (auto& image : asset->images) {
        load_image(asset, image);
    }
    for (auto& material : asset->materials) {
        load_material(asset, material);
    }
    for (auto& mesh : asset->meshes) {
        load_mesh(asset, mesh);
    }
}

NewModel::~NewModel() {
    
}

void NewModel::load_image(const std::unique_ptr<fastgltf::Asset>& asset, fastgltf::Image& image) {
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
}

