#include "rendering_system.hpp"

#include "../data/entity.hpp"
#include "../data/components.hpp"

#include "../rendering/task.hpp"
#include "../rendering/basic_forward.hpp"
namespace dare {
    RenderingSystem::RenderingSystem(std::unique_ptr<Window>& window) : window{window} {
        // setup context
        this->context.context = daxa::create_context({
            .enable_validation = false,
        });

        this->context.device = this->context.context.create_device({
            .debug_name = "device"
        });

        this->context.swapchain = this->context.device.create_swapchain({
            .native_window = window->get_native_handle(),
            .present_mode = daxa::PresentMode::DO_NOT_WAIT_FOR_VBLANK,
            .image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST,
            .debug_name = "swapchain",
        });

        this->context.pipeline_compiler = this->context.device.create_pipeline_compiler({
            .shader_compile_options = {
                .root_paths = {
                    ".out/release/vcpkg_installed/x64-linux/include",
                    "build/vcpkg_installed/x64-linux/include",
                    "vcpkg_installed/x64-linux/include",
                    "shaders",
                    "../shaders",
                    "include",
                },
                .language = daxa::ShaderLanguage::GLSL,
            },
            .debug_name = "pipeline_compiler",
        });

        // setup imgui
        ImGui::CreateContext();
        imgui_renderer = daxa::ImGuiRenderer({
            .device = this->context.device,
            .pipeline_compiler = this->context.pipeline_compiler,
            .format = this->context.swapchain.get_format(),
        });

        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    
        this->camera_buffer = std::make_unique<Buffer<CameraInfo>>(this->context.device);
        this->task = std::make_unique<BasicForward>(context);
    }

    RenderingSystem::~RenderingSystem() {
        this->context.device.wait_idle();
        this->context.device.collect_garbage();
        ImGui_ImplGlfw_Shutdown();
    }

    void RenderingSystem::draw(const std::shared_ptr<Scene>& scene, ControlledCamera3D& camera) {
        auto swapchain_image = this->context.swapchain.acquire_next_image();
        if (swapchain_image.is_empty())
        {
            return;
        }

        u32 size_x = this->context.swapchain.get_surface_extent().x;
        u32 size_y = this->context.swapchain.get_surface_extent().y;

        auto cmd_list = this->context.device.create_command_list({
            .debug_name = APPNAME_PREFIX("cmd_list"),
        });

        {
            glm::mat4 view = camera.camera.get_view();

            CameraInfo camera_info {
                .projection_matrix = *reinterpret_cast<const f32mat4x4*>(&camera.camera.proj_mat),
                .view_matrix = *reinterpret_cast<const f32mat4x4*>(&view),
                .position = *reinterpret_cast<const f32vec3*>(&camera.pos)
            };

            camera_buffer->update(cmd_list, camera_info);
        }

        cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::COLOR_ATTACHMENT_OUTPUT_WRITE,
            .before_layout = daxa::ImageLayout::UNDEFINED,
            .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
            .image_id = swapchain_image,
        });

        this->task->render(cmd_list, scene, camera_buffer->buffer_address);

        imgui_renderer.record_commands(ImGui::GetDrawData(), cmd_list, swapchain_image, size_x, size_y);

        cmd_list.pipeline_barrier_image_transition({
            .awaited_pipeline_access = daxa::AccessConsts::ALL_GRAPHICS_READ_WRITE,
            .before_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
            .after_layout = daxa::ImageLayout::PRESENT_SRC,
            .image_id = swapchain_image,
        });

        cmd_list.complete();

        this->context.device.submit_commands({
            .command_lists = {std::move(cmd_list)},
            .wait_binary_semaphores = {this->context.swapchain.get_acquire_semaphore()},
            .signal_binary_semaphores = {this->context.swapchain.get_present_semaphore()},
            .signal_timeline_semaphores = {{this->context.swapchain.get_gpu_timeline_semaphore(), this->context.swapchain.get_cpu_timeline_value()}},
        });

        this->context.device.present_frame({
            .wait_binary_semaphores = {this->context.swapchain.get_present_semaphore()},
            .swapchain = this->context.swapchain,
        });
    }

    void RenderingSystem::resize(u32 sx, u32 sy) {
        this->context.swapchain.resize();
        this->size = { static_cast<f32>(sx), static_cast<f32>(sy) };
        this->task->resize(sx, sy);
    }

    auto RenderingSystem::get_render_image() -> daxa::ImageId {
        return this->task->get_color_image();
    }
}