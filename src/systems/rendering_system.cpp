#include "rendering_system.hpp"

namespace dare {
    RenderingSystem::RenderingSystem(std::unique_ptr<Window>& window) : window{window} {
        // setup context
        this->context.context = daxa::create_context({
            .enable_validation = false,
        });

        this->context.device = this->context.context.create_device({
            .use_scalar_layout = true,
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
        ImGui_ImplGlfw_InitForVulkan(window->glfw_window_ptr, true);
        imgui_renderer = daxa::ImGuiRenderer({
            .device = this->context.device,
            .pipeline_compiler = this->context.pipeline_compiler,
            .format = this->context.swapchain.get_format(),
        });

        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    }

    RenderingSystem::~RenderingSystem() {
        this->context.device.wait_idle();
        this->context.device.collect_garbage();
        ImGui_ImplGlfw_Shutdown();
    }

    void RenderingSystem::draw(const std::shared_ptr<Scene>& scene) {

    }
}