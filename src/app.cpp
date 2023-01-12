#include "app.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <thread>

App::App() : AppWindow<App>("poggers")  {
    this->context = daxa::create_context({
        .enable_validation = true
    });

    this->device = context.create_device({
        .debug_name = "my gpu"
    });

    this->swapchain = device.create_swapchain({
        .native_window = get_native_handle(glfw_window_ptr),
        .present_mode = daxa::PresentMode::DO_NOT_WAIT_FOR_VBLANK,
        .image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST,
        .debug_name = "swapchain"
    });

    this->pipeline_manager = daxa::PipelineManager({
        .device = device,
        .shader_compile_options = {
            .root_paths = {
                "/shaders",
                "../shaders",
                "../../shaders",
                "../../../shaders",
                "shaders",
                DAXA_SHADER_INCLUDE_DIR,
            },
            .language = daxa::ShaderLanguage::GLSL,
            .enable_debug_info = true,
        },
        .debug_name = "pipeline_manager",
    });

    this->raster_pipeline = pipeline_manager.add_raster_pipeline({
        .vertex_shader_info = {.source = daxa::ShaderFile{"draw.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_VERT"}}}},
        .fragment_shader_info = {.source = daxa::ShaderFile{"draw.glsl"}, .compile_options = {.defines = {daxa::ShaderDefine{"DRAW_FRAG"}}}},
        .color_attachments = {{ .format = swapchain.get_format() }},
        .depth_test = {
            .depth_attachment_format = daxa::Format::D24_UNORM_S8_UINT,
            .enable_depth_test = true,
            .enable_depth_write = true,
        },
        .raster = {
            .face_culling = daxa::FaceCullFlagBits::NONE
        },
        .push_constant_size = sizeof(DrawPush),
        .debug_name = "raster_pipeline",
    }).value();

    this->depth_image = device.create_image({
        .format = daxa::Format::D24_UNORM_S8_UINT,
        .aspect = daxa::ImageAspectFlagBits::DEPTH | daxa::ImageAspectFlagBits::STENCIL,
        .size = {size_x, size_y, 1},
        .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT,
    });

    this->model = std::make_unique<Model>(device, "./assets/models/Sponza/Sponza.gltf");
    
    camera.camera.resize(size_x, size_y);
}

App::~App() {
    device.wait_idle();
    device.collect_garbage();
    device.destroy_image(depth_image);
}

bool App::update() {
    glfwPollEvents();
    if (glfwWindowShouldClose(glfw_window_ptr)) {
        return true;
    }

    if (!minimized) {
        on_update();
    } else {
        using namespace std::literals;
        std::this_thread::sleep_for(1ms);
    }

    return false;
}

void App::ui_update() {}

void App::on_update() {
    auto now = Clock::now();
    elapsed_s = std::chrono::duration<f32>(now - prev_time).count();
    prev_time = now;

    ui_update();

    camera.camera.set_pos(camera.pos);
    camera.camera.set_rot(camera.rot.x, camera.rot.y);
    camera.update(elapsed_s);

    render();
}

void App::render() {
    daxa::ImageId swapchain_image = swapchain.acquire_next_image();
    if(swapchain_image.is_empty()) {
        return;
    }

    daxa::CommandList cmd_list = device.create_command_list({ .debug_name = "main loop cmd list" });

    cmd_list.pipeline_barrier_image_transition({
        .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
        .before_layout = daxa::ImageLayout::UNDEFINED,
        .after_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
        .image_id = swapchain_image
    });

    cmd_list.pipeline_barrier_image_transition({
        .waiting_pipeline_access = daxa::AccessConsts::TRANSFER_WRITE,
        .before_layout = daxa::ImageLayout::UNDEFINED,
        .after_layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
        .image_slice = {.image_aspect = daxa::ImageAspectFlagBits::DEPTH | daxa::ImageAspectFlagBits::STENCIL},
        .image_id = depth_image,
    });

    cmd_list.begin_renderpass({
        .color_attachments = {{
            .image_view = swapchain_image.default_view(),
            .load_op = daxa::AttachmentLoadOp::CLEAR,
            .clear_value = std::array<daxa::f32, 4>{0.2f, 0.4f, 1.0f, 1.0f},
        }},
        .depth_attachment = {{
            .image_view = depth_image.default_view(),
            .load_op = daxa::AttachmentLoadOp::CLEAR,
            .clear_value = daxa::DepthValue{1.0f, 0},
        }},
        .render_area = {.x = 0, .y = 0, .width = size_x, .height = size_y},
    });
    cmd_list.set_pipeline(*raster_pipeline);

    glm::mat4 m = glm::translate(glm::mat4(1.0f), {0.0f, 5.0f, 0.0f}) * glm::toMat4(glm::quat({0.0f, glm::radians(90.0f), glm::radians(180.0f)})) * glm::scale(glm::mat4(1.0f), {0.03f, 0.03f, 0.03f});

    glm::mat4 vp =  camera.camera.get_vp() * m;

    DrawPush push;
    push.mvp = *reinterpret_cast<const f32mat4x4*>(&vp);

    model->draw(cmd_list, push);

    cmd_list.end_renderpass();

    cmd_list.pipeline_barrier_image_transition({
        .awaited_pipeline_access = daxa::AccessConsts::ALL_GRAPHICS_READ_WRITE,
        .before_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
        .after_layout = daxa::ImageLayout::PRESENT_SRC,
        .image_id = swapchain_image
    });

    cmd_list.complete();

    device.submit_commands({
        .command_lists = {std::move(cmd_list)},
        .wait_binary_semaphores = {swapchain.get_acquire_semaphore()},
        .signal_binary_semaphores = {swapchain.get_present_semaphore()},
        .signal_timeline_semaphores = {{swapchain.get_gpu_timeline_semaphore(), swapchain.get_cpu_timeline_value()}},
    });

    device.present_frame({
        .wait_binary_semaphores = {swapchain.get_present_semaphore()},
        .swapchain = swapchain,
    });
}

void App::on_mouse_scroll(f32 x, f32 y) {}
void App::on_mouse_button(int key, int action) {}
void App::on_mouse_move(f32 x, f32 y) {
    if (!paused) {
        f32 center_x = static_cast<f32>(size_x / 2);
        f32 center_y = static_cast<f32>(size_y / 2);
        auto offset = f32vec2{x - center_x, center_y - y};
        camera.on_mouse_move(offset.x, offset.y);
        glfwSetCursorPos(glfw_window_ptr, static_cast<f64>(center_x), static_cast<f64>(center_y));
    }
}

void App::on_key(i32 key, i32 action) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        toggle_pause();
    }

    if (!paused) {
        camera.on_key(key, action);
    }
}

void App::toggle_pause() {
    glfwSetCursorPos(glfw_window_ptr, static_cast<f64>(size_x / 2), static_cast<f64>(size_y / 2));
    glfwSetInputMode(glfw_window_ptr, GLFW_CURSOR, paused ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    glfwSetInputMode(glfw_window_ptr, GLFW_RAW_MOUSE_MOTION, paused);
    paused = !paused;
}

void App::on_resize(u32 sx, u32 sy) {
    minimized = (sx == 0 || sy == 0);
    if (!minimized) {
        swapchain.resize();
        size_x = swapchain.get_surface_extent().x;
        size_y = swapchain.get_surface_extent().y;
        camera.camera.resize(size_x, size_y);
        device.destroy_image(depth_image);
        depth_image = device.create_image({
            .format = daxa::Format::D24_UNORM_S8_UINT,
            .aspect = daxa::ImageAspectFlagBits::DEPTH | daxa::ImageAspectFlagBits::STENCIL,
            .size = {size_x, size_y, 1},
            .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT,
        });

        on_update();
    }
}