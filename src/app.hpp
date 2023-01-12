#pragma once

#include <daxa/daxa.hpp>
#include <daxa/utils/pipeline_manager.hpp>
using namespace daxa::types;

#include "texture.hpp"
#include "model.hpp"

#include "window.hpp"
#include "camera.hpp"

#include <chrono>
using Clock = std::chrono::high_resolution_clock;

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

    Clock::time_point start = Clock::now(), prev_time = start;
    f32 elapsed_s = 1.0f;

    bool paused = true;

    ControlledCamera3D camera;

    std::shared_ptr<daxa::RasterPipeline> raster_pipeline;
    daxa::ImageId depth_image;

    std::unique_ptr<Model> model;
    std::unique_ptr<Texture> texture;
};