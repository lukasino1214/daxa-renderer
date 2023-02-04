#include "model_manager.hpp"
#include <cstring>
#include <iostream>

namespace dare {
    ModelManager::ModelManager(daxa::Device& device) : device{device} {
        this->position_buffer.element_size = sizeof(glm::vec3);
        this->position_buffer.buffer = device.create_buffer(daxa::BufferInfo{
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .size = static_cast<u32>(sizeof(glm::vec3) * STARTING_COUNT),
            .debug_name = "position_buffer",
        });

        this->normal_buffer.element_size = sizeof(glm::vec3);
        this->normal_buffer.buffer = device.create_buffer(daxa::BufferInfo{
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .size = static_cast<u32>(sizeof(glm::vec3) * STARTING_COUNT),
            .debug_name = "position_buffer",
        });

        this->uv_buffer.element_size = sizeof(glm::vec2);
        this->uv_buffer.buffer = device.create_buffer(daxa::BufferInfo{
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .size = static_cast<u32>(sizeof(glm::vec2) * STARTING_COUNT),
            .debug_name = "position_buffer",
        });

        this->index_buffer.element_size = INDEX_SIZE;
        this->index_buffer.buffer = device.create_buffer(daxa::BufferInfo{
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .size = static_cast<u32>(INDEX_SIZE * STARTING_COUNT),
            .debug_name = "index_buffer",
        });
    }

    ModelManager::~ModelManager() {
        device.destroy_buffer(position_buffer.buffer);
        device.destroy_buffer(normal_buffer.buffer);
        device.destroy_buffer(uv_buffer.buffer);
        device.destroy_buffer(index_buffer.buffer);
    }

    void ModelManager::reallocate_buffer(Buffer& buffer, u32 new_count) {
        daxa::BufferId new_buffer = device.create_buffer(daxa::BufferInfo{
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .size = static_cast<u32>(buffer.element_size * new_count),
            .debug_name = "new_buffer",
        });

        if(buffer.last_element_count != 0) {
            daxa::CommandList cmd_list = device.create_command_list({});
        
            cmd_list.copy_buffer_to_buffer({
                .src_buffer = buffer.buffer,
                .src_offset = 0,
                .dst_buffer = new_buffer,
                .dst_offset = 0,
                .size = buffer.element_size * buffer.last_element_count,
            });

            cmd_list.complete();
            device.submit_commands({
                .command_lists = {std::move(cmd_list)}
            });

            device.wait_idle();
        }

        device.destroy_buffer(buffer.buffer);
        buffer.buffer = new_buffer;
        buffer.allocated_element_count = new_count;
    }

    void ModelManager::copy_data_to(Buffer& buffer, u32 count, void* data) {
        auto staging_buffer = device.create_buffer({
            .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            .size = static_cast<u32>(buffer.element_size * count),
            .debug_name = "staging_buffer",
        });

        auto buffer_ptr = device.get_host_address_as<u8>(staging_buffer);
        std::memcpy(buffer_ptr, data, buffer.element_size * count);

        daxa::CommandList cmd_list = device.create_command_list({});

        cmd_list.copy_buffer_to_buffer({
            .src_buffer = staging_buffer,
            .src_offset = 0,
            .dst_buffer = buffer.buffer,
            .dst_offset = buffer.element_size * buffer.last_element_count,
            .size = buffer.element_size * count,
        });

        cmd_list.complete();
        device.submit_commands({
            .command_lists = {std::move(cmd_list)}
        });

        device.wait_idle();
        device.destroy_buffer(staging_buffer);
    }

    auto ModelManager::load_model(const std::string& path) -> std::shared_ptr<Model> {
        Model::LoadingResult result = Model::load_model(device, path);

        if(position_buffer.last_element_count + result.positions.size() > position_buffer.allocated_element_count) {
            reallocate_buffer(position_buffer, position_buffer.last_element_count + result.positions.size());
        }

        if(normal_buffer.last_element_count + result.normals.size() > normal_buffer.allocated_element_count) {
            reallocate_buffer(normal_buffer, normal_buffer.last_element_count + result.normals.size());
        }

        if(uv_buffer.last_element_count + result.uvs.size() > uv_buffer.allocated_element_count) {
            reallocate_buffer(uv_buffer, uv_buffer.last_element_count + result.uvs.size());
        }

        if(index_buffer.last_element_count + result.indices.size() > index_buffer.allocated_element_count) {
            reallocate_buffer(index_buffer, index_buffer.last_element_count + result.indices.size());
        }

        copy_data_to(position_buffer, result.positions.size(), result.positions.data());
        copy_data_to(normal_buffer, result.normals.size(), result.normals.data());
        copy_data_to(uv_buffer, result.uvs.size(), result.uvs.data());
        copy_data_to(index_buffer, result.indices.size(), result.indices.data());

        result.model->position_offset = position_buffer.last_element_count;
        result.model->normal_offset = normal_buffer.last_element_count;
        result.model->uv_offset = uv_buffer.last_element_count;
        result.model->index_offset = index_buffer.last_element_count;
        result.model->vertex_offset = position_buffer.last_element_count;
        result.model->device = device;
        result.model->model_manager = this;

        position_buffer.last_element_count += result.positions.size();
        normal_buffer.last_element_count += result.normals.size();
        uv_buffer.last_element_count += result.uvs.size();
        index_buffer.last_element_count += result.indices.size();

        return std::move(result.model);
    }
}