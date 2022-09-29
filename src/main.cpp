#include "window.hpp"

#include <daxa/context.hpp>
#include <daxa/daxa.hpp>
#include <thread>
#include <iostream>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#define APPNAME "Daxa Template App"
#define APPNAME_PREFIX(x) ("[" APPNAME "] " x)

#include <daxa/utils/imgui.hpp>
#include <imgui_impl_glfw.h>
#include <imgui.h>

using namespace daxa::types;
#include "../shaders/shared.inl"

using Clock = std::chrono::high_resolution_clock;

#include "graphics/model.hpp"
#include "graphics/camera.hpp"

#define DAXA_GLSL 1
struct App : AppWindow<App> {
    f64 current_frame = glfwGetTime();
    f64 last_frame = current_frame;
    f64 delta_time;

    daxa::Context daxa_ctx = daxa::create_context({
        .enable_validation = false,
    });
    
    daxa::Device device = daxa_ctx.create_device({
        .use_scalar_layout = true,
        .debug_name = APPNAME_PREFIX("device"),
    });

    daxa::Swapchain swapchain = device.create_swapchain({
        .native_window = get_native_handle(),
        .present_mode = daxa::PresentMode::DO_NOT_WAIT_FOR_VBLANK,
        .image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST,
        .debug_name = APPNAME_PREFIX("swapchain"),
    });

    daxa::ImageId depth_image = device.create_image({
        .format = daxa::Format::D24_UNORM_S8_UINT,
        .aspect = daxa::ImageAspectFlagBits::DEPTH | daxa::ImageAspectFlagBits::STENCIL,
        .size = {size_x, size_y, 1},
        .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT,
    });

    daxa::PipelineCompiler pipeline_compiler = device.create_pipeline_compiler({
        .shader_compile_options = {
            .root_paths = {
                ".out/release/vcpkg_installed/x64-linux/include",
                "build/vcpkg_installed/x64-linux/include",
                "vcpkg_installed/x64-linux/include",
                "shaders",
                "include",
            },
            .language = daxa::ShaderLanguage::GLSL,
        },
        .debug_name = APPNAME_PREFIX("pipeline_compiler"),
    });

    daxa::ImGuiRenderer imgui_renderer = create_imgui_renderer();
    auto create_imgui_renderer() -> daxa::ImGuiRenderer {
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForVulkan(glfw_window_ptr, true);
        return daxa::ImGuiRenderer({
            .device = device,
            .pipeline_compiler = pipeline_compiler,
            .format = swapchain.get_format(),
        });
    }

    // clang-format off
    daxa::RasterPipeline draw_pipeline = pipeline_compiler.create_raster_pipeline({
        .vertex_shader_info = {.source = daxa::ShaderFile{"draw.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_VERT"}}}},
        .fragment_shader_info = {.source = daxa::ShaderFile{"draw.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_FRAG"}}}},
        .color_attachments = {{.format = swapchain.get_format(), .blend = {.blend_enable = true, .src_color_blend_factor = daxa::BlendFactor::SRC_ALPHA, .dst_color_blend_factor = daxa::BlendFactor::ONE_MINUS_SRC_ALPHA}}},
        .depth_test = {
            .depth_attachment_format = daxa::Format::D24_UNORM_S8_UINT,
            .enable_depth_test = true,
            .enable_depth_write = true,
        },
        .raster = {
            .polygon_mode = daxa::PolygonMode::FILL,
            .face_culling = daxa::FaceCullFlagBits::BACK_BIT,
        },
        .push_constant_size = sizeof(DrawPush),
        .debug_name = APPNAME_PREFIX("raster_pipeline"),
    }).value();
    // clang-format on

    static inline constexpr u64 FRAMES_IN_FLIGHT = 1;
    daxa::TimelineSemaphore gpu_framecount_timeline_sema = device.create_timeline_semaphore(daxa::TimelineSemaphoreInfo{
        .initial_value = 0,
        .debug_name = APPNAME_PREFIX("gpu_framecount_timeline_sema"),
    });
    u64 cpu_framecount = FRAMES_IN_FLIGHT - 1;

    daxa::BinarySemaphore acquire_semaphore = device.create_binary_semaphore({.debug_name = APPNAME_PREFIX("acquire_semaphore")});
    daxa::BinarySemaphore present_semaphore = device.create_binary_semaphore({.debug_name = APPNAME_PREFIX("present_semaphore")});

    glm::mat4 m = glm::translate(glm::mat4(1.0f), {0.0f, 5.0f, 0.0f}) * glm::toMat4(glm::quat({0.0f, glm::radians(90.0f), glm::radians(180.0f)})) * glm::scale(glm::mat4(1.0f), {0.03f, 0.03f, 0.03f});

    u32 current_camera = 0;
    std::vector<ControlledCamera3D> cameras{2};

    std::vector<Model> models;

    daxa::BufferId camera_info_buffer = device.create_buffer({
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
        .size = static_cast<u32>(sizeof(CameraInfo)),
    });

    u64 camera_info_buffer_address = device.buffer_reference(camera_info_buffer);

    daxa::BufferId object_info_buffer = device.create_buffer({
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
        .size = static_cast<u32>(sizeof(ObjectInfo)),
    });

    u64 object_info_buffer_address = device.buffer_reference(object_info_buffer);

    daxa::BufferId lights_info_buffer = device.create_buffer({
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
        .size = static_cast<u32>(sizeof(LightsInfo)),
    });

    u64 lights_info_buffer_address = device.buffer_reference(lights_info_buffer);

    bool paused = true;

    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

    u32 viewport_size_x = 1280, viewport_size_y = 720;
    bool resized = true;

    App() : AppWindow<App>(APPNAME) {}

    ~App() {
        device.wait_idle();
        device.collect_garbage();
        device.destroy_image(depth_image);
        device.destroy_buffer(camera_info_buffer);
        device.destroy_buffer(object_info_buffer);
        device.destroy_buffer(lights_info_buffer);
        ImGui_ImplGlfw_Shutdown();
        for (auto & model : models) {
            model.destroy(device);
        }
    }

    bool update() {
        glfwPollEvents();
        if (glfwWindowShouldClose(glfw_window_ptr)) {
            return true;
        }

        if (!minimized) {
            draw();
        } else {
            using namespace std::literals;
            std::this_thread::sleep_for(1ms);
        }

        return false;
    }

    void ui_update() {
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Debug
        {
            ImGui::Begin("Debug");
            ImGui::Text("CPU Frame Count: %i", static_cast<i32>(cpu_framecount));
            ImGui::Text("Frame time: %f", delta_time);
            ImGui::Text("Frame Per Second: %f", 1.0 / delta_time);
            ImGui::End();
        }

        // Cameras
        {
            ImGui::Begin("Camera");
            if(ImGui::Button("Add Camera")) {
                cameras.push_back({});
            }

            for(usize i = 0; i < cameras.size(); i++) {
                if(ImGui::Button((std::string("camera: ") + std::to_string(i + 1)).c_str())) {
                    current_camera = i;
                }
            }
            ImGui::End();
        }

        ImGui::Render();
    }

    void draw() {
        current_frame = glfwGetTime();
        delta_time = current_frame - last_frame;
        last_frame = current_frame;

        ui_update();

        cameras[current_camera].camera.resize(static_cast<i32>(viewport_size_x), static_cast<i32>(viewport_size_y));
        cameras[current_camera].camera.set_pos(cameras[current_camera].pos);
        cameras[current_camera].camera.set_rot(cameras[current_camera].rot.x, cameras[current_camera].rot.y);
        cameras[current_camera].update(delta_time);

        if (pipeline_compiler.check_if_sources_changed(draw_pipeline)) {
            auto new_pipeline = pipeline_compiler.recreate_raster_pipeline(draw_pipeline);
            std::cout << new_pipeline.to_string() << std::endl;
            if (new_pipeline.is_ok()) {
                draw_pipeline = new_pipeline.value();
            }
        }

        auto swapchain_image = swapchain.acquire_next_image(acquire_semaphore);

        auto cmd_list = device.create_command_list({
            .debug_name = APPNAME_PREFIX("cmd_list"),
        });

        /*glm::mat4 view = cameras[current_camera].camera.get_view();

        CameraInfo camera_info {
            .projection_matrix = *reinterpret_cast<const f32mat4x4*>(&cameras[current_camera].camera.proj_mat),
            .view_matrix = *reinterpret_cast<const f32mat4x4*>(&view),
            .position = *reinterpret_cast<const f32vec3*>(&cameras[current_camera].pos)
        };

        auto staging_buffer_ptr = device.map_memory_as<u8>(camera_info_buffer);
        std::memcpy(staging_buffer_ptr, &camera_info, sizeof(CameraInfo));
        device.unmap_memory(camera_info_buffer);

        cmd_list.pipeline_barrier({ // Maybe this
            .awaited_pipeline_access = daxa::AccessConsts::HOST_WRITE,
            .waiting_pipeline_access = daxa::AccessConsts::ALL_GRAPHICS_READ_WRITE,
        });*/

        cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::COLOR_ATTACHMENT_OUTPUT_WRITE,
            .before_layout = daxa::ImageLayout::UNDEFINED,
            .after_layout = daxa::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
            .image_id = swapchain_image,
        });

        cmd_list.pipeline_barrier_image_transition({
            .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
            .before_layout = daxa::ImageLayout::UNDEFINED,
            .after_layout = daxa::ImageLayout::DEPTH_ATTACHMENT_OPTIMAL,
            .image_id = depth_image,
            .image_slice = {.image_aspect = daxa::ImageAspectFlagBits::DEPTH | daxa::ImageAspectFlagBits::STENCIL},
        });

        f32 factor = 32.0f;

        cmd_list.begin_renderpass({
            .color_attachments = {{
                .image_view = swapchain_image.default_view(),
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = std::array<f32, 4>{0.2f / factor, 0.4f / factor, 1.0f / factor, 1.0f},
            }},
            .depth_attachment = {{
                .image_view = depth_image.default_view(),
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = daxa::DepthValue{1.0f, 0},
            }},
            .render_area = {.x = 0, .y = 0, .width = size_x, .height = size_y},
        });
        cmd_list.set_pipeline(draw_pipeline);

        glm::mat4 temp_mvp = cameras[current_camera].camera.get_vp();

        for (auto & model : models) {
            model.bind_index_buffer(cmd_list);
            model.draw(cmd_list, temp_mvp, cameras[current_camera].pos, camera_info_buffer_address, object_info_buffer_address, lights_info_buffer_address);
        }
        cmd_list.end_renderpass();

        imgui_renderer.record_commands(ImGui::GetDrawData(), cmd_list, swapchain_image, size_x, size_y);

        cmd_list.pipeline_barrier_image_transition({
            .awaited_pipeline_access = daxa::AccessConsts::ALL_GRAPHICS_READ_WRITE,
            .before_layout = daxa::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
            .after_layout = daxa::ImageLayout::PRESENT_SRC,
            .image_id = swapchain_image,
        });

        cmd_list.complete();

        cpu_framecount++;
        device.submit_commands({
            .command_lists = {std::move(cmd_list)},
            .wait_binary_semaphores = {acquire_semaphore},
            .signal_binary_semaphores = {present_semaphore},
            .signal_timeline_semaphores = {{gpu_framecount_timeline_sema, cpu_framecount}},
        });

        device.present_frame({
            .wait_binary_semaphores = {present_semaphore},
            .swapchain = swapchain,
        });

        gpu_framecount_timeline_sema.wait_for_value(cpu_framecount - 1);
    }

    void setup() {
        models.push_back(Model::load(device, "assets/models/Sponza/Sponza.gltf"));

        std::cout << "Loading models done!" << std::endl;

        {
            glm::mat4 normal_matrix = glm::transpose(glm::inverse(m));

            ObjectInfo object_info {
                .model_matrix = *reinterpret_cast<const f32mat4x4 *>(&m),
                .normal_matrix = *reinterpret_cast<const f32mat4x4 *>(&normal_matrix)
            };

            auto staging_buffer_ptr = device.map_memory_as<u8>(object_info_buffer);
            std::memcpy(staging_buffer_ptr, &object_info, sizeof(ObjectInfo));
            device.unmap_memory(object_info_buffer);
        }

        LightsInfo lights_info;
        lights_info.num_point_lights = 3;

        lights_info.point_lights[0] = PointLight {
            .position = { 0.0f, -1.0f, 0.0f },
            .color = { 1.0f, 0.0f, 0.0f },
            .intensity = 128.0f,
        };

        lights_info.point_lights[1] = PointLight {
            .position = { 0.0f, -1.0f, 20.0f },
            .color = { 0.0f, 0.0f, 1.0f },
            .intensity = 128.0f,
        };

        lights_info.point_lights[2] = PointLight {
            .position = { 0.0f, -1.0f, -20.0f },
            .color = { 0.0f, 1.0f, 0.0f },
            .intensity = 128.0f,
        };

        {
            auto staging_buffer_ptr = device.map_memory_as<u8>(lights_info_buffer);
            std::memcpy(staging_buffer_ptr, &lights_info, sizeof(LightsInfo));
            device.unmap_memory(lights_info_buffer);

        }

        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    }

    void on_mouse_button(i32, i32) {}
    void on_mouse_move(f32 x, f32 y) {
        if (!paused) {
            f32 center_x = static_cast<f32>(size_x / 2);
            f32 center_y = static_cast<f32>(size_y / 2);
            auto offset = f32vec2{x - center_x, center_y - y};
            cameras[current_camera].on_mouse_move(offset.x, offset.y);
            set_mouse_pos(center_x, center_y);
        }
    }

    void on_key(i32 key, i32 action) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            toggle_pause();
        }

        if (!paused) {
            cameras[current_camera].on_key(key, action);
        }
    }

    void on_resize(u32 sx, u32 sy) {
        minimized = (sx == 0 || sy == 0);
        if (!minimized) {
            swapchain.resize();
            size_x = swapchain.info().width;
            size_y = swapchain.info().height;
            device.destroy_image(depth_image);
            depth_image = device.create_image({
                .format = daxa::Format::D24_UNORM_S8_UINT,
                .aspect = daxa::ImageAspectFlagBits::DEPTH | daxa::ImageAspectFlagBits::STENCIL,
                .size = {size_x, size_y, 1},
                .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT,
            });
            draw();
        }
    }

    void toggle_pause() {
        set_mouse_capture(paused);
        paused = !paused;
    }
};

int main() {
    App app = {};
    app.setup();
    while (true) {
        if (app.update())
            break;
    }
}