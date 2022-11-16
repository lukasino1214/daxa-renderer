#pragma once

#include <daxa/daxa.hpp>
using namespace daxa::types;

#include "../utils/utils.hpp"

namespace dare {
    template<typename T>
    struct Buffer {
        daxa::Device& device;
        daxa::BufferId buffer_id;
        u64 buffer_address;

        Buffer(daxa::Device& device, const std::string& debug_name = "created buffer type of " + std::string{type_name<T>()}, daxa::MemoryFlags memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY) : device{device} {
            this->buffer_id = device.create_buffer({
                .memory_flags = memory_flags,
                .size = sizeof(T),
                .debug_name = debug_name,
            });

            this->buffer_address = device.buffer_reference(buffer_id);
        }
        ~Buffer() {
            device.destroy_buffer(buffer_id);
        }

        void update(const T& data) {
            auto cmd_list = device.create_command_list({
                .debug_name = "updating buffer " + std::string{type_name<T>()},
            });

            daxa::BufferId staging_buffer = device.create_buffer({
                .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .size = static_cast<u32>(sizeof(T)),
            });

            auto staging_buffer_ptr = device.map_memory_as<u8>(staging_buffer);
            std::memcpy(staging_buffer_ptr, &data, sizeof(T));
            device.unmap_memory(staging_buffer);

            cmd_list.pipeline_barrier({
                .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
                .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_READ,
            });

            cmd_list.copy_buffer_to_buffer({
                .src_buffer = staging_buffer,
                .dst_buffer = buffer_id,
                .size = sizeof(T),
            });

            cmd_list.pipeline_barrier({
                .awaited_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
                .waiting_pipeline_access = daxa::AccessConsts::READ,
            });

            cmd_list.complete();
            device.submit_commands({
                .command_lists = {std::move(cmd_list)}
            });
            device.wait_idle();

            device.destroy_buffer(staging_buffer);
        }

        void update(daxa::CommandList& cmd_list, const T& data) {
            daxa::BufferId staging_buffer = device.create_buffer({
                .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .size = static_cast<u32>(sizeof(T)),
            });

            cmd_list.destroy_buffer_deferred(staging_buffer);

            auto staging_buffer_ptr = device.map_memory_as<u8>(staging_buffer);
            std::memcpy(staging_buffer_ptr, &data, sizeof(T));
            device.unmap_memory(staging_buffer);

            cmd_list.pipeline_barrier({
                .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
                .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_READ,
            });

            cmd_list.copy_buffer_to_buffer({
                .src_buffer = staging_buffer,
                .dst_buffer = buffer_id,
                .size = sizeof(T),
            });

            cmd_list.pipeline_barrier({
                .awaited_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
                .waiting_pipeline_access = daxa::AccessConsts::READ,
            });
        }
    };
}