#include "model_manager.hpp"
#include <cstring>

namespace dare {
    ModelManager::ModelManager(daxa::Device& device) : device{device} {
        this->vertex_buffer = device.create_buffer(daxa::BufferInfo{
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .size = static_cast<u32>(VERTEX_SIZE * STARTING_COUNT),
            .debug_name = "vertex_buffer",
        });

        this->index_buffer = device.create_buffer(daxa::BufferInfo{
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .size = static_cast<u32>(INDEX_SIZE * STARTING_COUNT),
            .debug_name = "index_buffer",
        });
    }

    ModelManager::~ModelManager() {
        device.destroy_buffer(vertex_buffer);
        device.destroy_buffer(index_buffer);
    }

    void ModelManager::reallocate_buffers(u32 vertex_count, u32 index_count) {
        /*device.destroy_buffer(vertex_buffer);
        device.destroy_buffer(index_buffer);
        
        this->vertex_buffer = device.create_buffer(daxa::BufferInfo{
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .size = static_cast<u32>(VERTEX_SIZE * (allocated_vertex_count + vertex_count)),
            .debug_name = "vertex_buffer",
        });

        this->index_buffer = device.create_buffer(daxa::BufferInfo{
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .size = static_cast<u32>(INDEX_SIZE * (allocated_index_count + index_count)),
            .debug_name = "index_buffer",
        });*/

        daxa::BufferId new_vertex_buffer = device.create_buffer(daxa::BufferInfo{
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .size = static_cast<u32>(VERTEX_SIZE * vertex_count),
            .debug_name = "vertex_buffer",
        });

        daxa::BufferId new_index_buffer = device.create_buffer(daxa::BufferInfo{
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .size = static_cast<u32>(INDEX_SIZE * index_count),
            .debug_name = "vertex_buffer",
        });

        if(last_vertex_count != 0 && last_index_count != 0) {
            daxa::CommandList cmd_list = device.create_command_list({});
        
            cmd_list.copy_buffer_to_buffer({
                .src_buffer = vertex_buffer,
                .src_offset = 0,
                .dst_buffer = new_vertex_buffer,
                .dst_offset = 0,
                .size = VERTEX_SIZE * last_vertex_count,
            });

            cmd_list.copy_buffer_to_buffer({
                .src_buffer = index_buffer,
                .src_offset = 0,
                .dst_buffer = new_index_buffer,
                .dst_offset = 0,
                .size = INDEX_SIZE * last_index_count,
            });

            cmd_list.complete();
            device.submit_commands({
                .command_lists = {std::move(cmd_list)}
            });

            device.wait_idle();
        }

        device.destroy_buffer(vertex_buffer);
        device.destroy_buffer(index_buffer);

        vertex_buffer = new_vertex_buffer;
        index_buffer = new_index_buffer;

        allocated_vertex_count = vertex_count;
        allocated_index_count = index_count;
    }

    auto  ModelManager::load_model(const std::string& path) -> std::shared_ptr<Model> {
        Model::LoadingResult result = Model::load_model(device, path);

        if(last_vertex_count + result.vertices.size() > allocated_vertex_count || last_index_count + result.indices.size() > allocated_index_count) {
            reallocate_buffers(last_vertex_count + result.vertices.size(), last_index_count + result.indices.size());
        }

        auto vertex_staging_buffer = device.create_buffer({
            .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            .size = static_cast<u32>(VERTEX_SIZE * result.vertices.size()),
            .debug_name = "vertex_staging_buffer",
        });

        auto index_staging_buffer = device.create_buffer({
            .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            .size = static_cast<u32>(INDEX_SIZE * result.indices.size()),
            .debug_name = "index_staging_buffer",
        });

        {
            auto buffer_ptr = device.get_host_address_as<DrawVertex>(vertex_staging_buffer);
            std::memcpy(buffer_ptr, result.vertices.data(), VERTEX_SIZE * result.vertices.size());
        }

        {
            auto buffer_ptr = device.get_host_address_as<u32>(index_staging_buffer);
            std::memcpy(buffer_ptr, result.indices.data(), INDEX_SIZE * result.indices.size());
        }

        daxa::CommandList cmd_list = device.create_command_list({});

        cmd_list.copy_buffer_to_buffer({
            .src_buffer = vertex_staging_buffer,
            .src_offset = 0,
            .dst_buffer = vertex_buffer,
            .dst_offset = last_vertex_count,
            .size = VERTEX_SIZE * result.vertices.size(),
        });

        cmd_list.copy_buffer_to_buffer({
            .src_buffer = index_staging_buffer,
            .src_offset = 0,
            .dst_buffer = index_buffer,
            .dst_offset = last_index_count,
            .size = INDEX_SIZE * result.indices.size(),
        });

        cmd_list.complete();
        device.submit_commands({
            .command_lists = {std::move(cmd_list)}
        });

        device.wait_idle();
        device.destroy_buffer(vertex_staging_buffer);
        device.destroy_buffer(index_staging_buffer);

        for(auto& primitive : result.model->primitives) {
            primitive.first_index += last_index_count;
            primitive.first_vertex += last_vertex_count;
        }

        last_vertex_count += result.vertices.size();
        last_index_count += result.indices.size();

        result.model->vertex_buffer = vertex_buffer;
        result.model->vertex_buffer_address = device.get_device_address(vertex_buffer);
        result.model->index_buffer = index_buffer;
        result.model->device = device;

        return std::move(result.model);
    }
}