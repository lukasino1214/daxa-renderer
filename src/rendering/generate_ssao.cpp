#include "generate_ssao.hpp"

#include <random>
#include <glm/glm.hpp>
#include <vector>
#include <cstring>

#include "../../shaders/shared.inl"

namespace dare {
    f32 lerp(f32 a, f32 b, f32 f) {
		return a + f * (b - a);
	}

    auto SSAO::generate(daxa::Device& device) -> SSAO::SSAOData {
        std::default_random_engine rnd_engine((unsigned)time(nullptr));
		std::uniform_real_distribution<f32> rnd_dist(0.0f, 1.0f);

        std::vector<glm::vec4> ssao_kernel(SSAO_KERNEL_SIZE);
		for (u32 i = 0; i < SSAO_KERNEL_SIZE; i++) {
			glm::vec3 sample(rnd_dist(rnd_engine) * 2.0 - 1.0, rnd_dist(rnd_engine) * 2.0 - 1.0, rnd_dist(rnd_engine));
			sample = glm::normalize(sample);
			sample *= rnd_dist(rnd_engine);
			f32 scale = float(i) / float(SSAO_KERNEL_SIZE);
			scale = lerp(0.1f, 1.0f, scale * scale);
			ssao_kernel[i] = glm::vec4(sample * scale, 0.0f);
		}

        auto cmd_list = device.create_command_list({
            .debug_name = APPNAME_PREFIX("cmd_list - ssao"),
        });

        daxa::BufferId ssao_kernel_buffer = device.create_buffer({
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .size = sizeof(SSAOKernel)
        });

        daxa::BufferId staging_ssao_kernel_buffer = device.create_buffer({
            .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            .size = sizeof(SSAOKernel)
        });

        {
            auto buffer_ptr = device.get_host_address_as<SSAOKernel>(staging_ssao_kernel_buffer);
            std::memcpy(buffer_ptr, ssao_kernel.data(), sizeof(SSAOKernel));
        }

        cmd_list.pipeline_barrier({
            .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
            .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_READ,
        });

        cmd_list.copy_buffer_to_buffer({
            .src_buffer = staging_ssao_kernel_buffer,
            .dst_buffer = ssao_kernel_buffer,
            .size = static_cast<u32>(sizeof(SSAOKernel)),
        });

        std::vector<glm::vec4> ssao_noise(SSAO_NOISE_DIM * SSAO_NOISE_DIM);
		for (u32 i = 0; i < static_cast<uint32_t>(ssao_noise.size()); i++) {
			ssao_noise[i] = glm::vec4(rnd_dist(rnd_engine) * 2.0f - 1.0f, rnd_dist(rnd_engine) * 2.0f - 1.0f, 0.0f, 0.0f);
		}

        daxa::ImageId ssao_noise_image = device.create_image({
            .dimensions = 2,
            .format = daxa::Format::R32G32B32A32_SFLOAT,
            .aspect = daxa::ImageAspectFlagBits::COLOR,
            .size = { SSAO_NOISE_DIM, SSAO_NOISE_DIM, 1 },
            .mip_level_count = 1,
            .array_layer_count = 1,
            .sample_count = 1,
            .usage = daxa::ImageUsageFlagBits::SHADER_READ_ONLY | daxa::ImageUsageFlagBits::TRANSFER_DST,
            .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY
        });

        daxa::BufferId staging_ssao_noise_buffer = device.create_buffer({
            .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            .size = static_cast<u32>(ssao_noise.size() * sizeof(glm::vec4))
        });

        {
            auto buffer_ptr = device.get_host_address_as<glm::vec4>(staging_ssao_kernel_buffer);
            std::memcpy(buffer_ptr, ssao_noise.data(), static_cast<u32>(ssao_noise.size() * sizeof(glm::vec4)));
        }


        cmd_list.pipeline_barrier_image_transition({
            .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
            .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_READ,
            .before_layout = daxa::ImageLayout::UNDEFINED,
            .after_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
            .image_slice = {
                .base_mip_level = 0,
                .level_count = 1,
                .base_array_layer = 0,
                .layer_count = 1
            },
            .image_id = ssao_noise_image,
        });
        cmd_list.copy_buffer_to_image({
            .buffer = staging_ssao_noise_buffer,
            .buffer_offset = 0,
            .image = ssao_noise_image,
            .image_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
            .image_slice = {
                .image_aspect = daxa::ImageAspectFlagBits::COLOR,
                .mip_level = 0,
                .base_array_layer = 0,
                .layer_count = 1,
            },
            .image_offset = { 0, 0, 0 },
            .image_extent = { static_cast<u32>(SSAO_NOISE_DIM), static_cast<u32>(SSAO_NOISE_DIM), 1 }
        });
        cmd_list.pipeline_barrier({
            .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
            .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_READ,
        });

        cmd_list.complete();
        device.submit_commands({
            .command_lists = {std::move(cmd_list)}
        });

        device.wait_idle();
        device.destroy_buffer(staging_ssao_kernel_buffer);
        device.destroy_buffer(staging_ssao_noise_buffer);

        return SSAO::SSAOData {
            .ssao_noise = ssao_noise_image,
            .ssao_kernel = ssao_kernel_buffer
        };
    }

    void SSAO::cleanup(daxa::Device& device, SSAO::SSAOData& data) {
        device.destroy_image(data.ssao_noise);
        device.destroy_buffer(data.ssao_kernel);
    }
}