#pragma once

#include <daxa/daxa.hpp>
#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/imgui.hpp>
#include <imgui_impl_glfw.h>
#include <imgui.h>
using namespace daxa::types;

#include "graphics/texture.hpp"
#include "graphics/model.hpp"

#include "window.hpp"
#include "graphics/camera.hpp"

#include <chrono>
using Clock = std::chrono::high_resolution_clock;

#include "data/scene.hpp"
#include "panels/scene_hiearchy.hpp"
using namespace dare;

#include "rendering/bloom_renderer.hpp"
#include "rendering/ssao_renderer.hpp"

struct App : AppWindow<App> {
    App();
    ~App();

    bool update();
    void ui_update();
    void on_update();
    void render();

    void on_mouse_move(f32 x, f32 y);
    void on_mouse_scroll(f32 x, f32 y);
    void on_mouse_button(int key, int action);
    void on_key(int key, int action);
    void on_resize(u32 sx, u32 sy);
    void toggle_pause();

    daxa::Context context;
    daxa::Device device;
    daxa::Swapchain swapchain;
    daxa::PipelineManager pipeline_manager;
    daxa::ImGuiRenderer imgui_renderer;

    Clock::time_point start = Clock::now(), prev_time = start;
    f32 elapsed_s = 1.0f;

    bool paused = true;

    ControlledCamera3D camera;

    std::shared_ptr<daxa::RasterPipeline> deffered_pipeline;
    std::shared_ptr<daxa::RasterPipeline> composition_pipeline;

    daxa::ImageId depth_image;
    daxa::ImageId albedo_image;
    daxa::ImageId normal_image;
    daxa::ImageId emissive_image;

    daxa::SamplerId sampler;

    u32 half_size_x = size_x / 2;
    u32 half_size_y = size_y / 2;

    std::unique_ptr<Buffer<CameraInfo>> camera_buffer;
    std::shared_ptr<SceneHiearchyPanel> scene_hiearchy;

    std::unique_ptr<BloomRenderer> bloom_renderer;
    std::unique_ptr<SSAORenderer> ssao_renderer;

    std::shared_ptr<Scene> scene;
};