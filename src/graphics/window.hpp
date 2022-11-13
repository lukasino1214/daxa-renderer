#pragma once
#include <daxa/daxa.hpp>
using namespace daxa::types;

#include <GLFW/glfw3.h>

namespace dare {
    struct Window {
        GLFWwindow * glfw_window_ptr;
        u32 size_x, size_y;
        bool minimized = false;

        explicit Window(char const * window_name, u32 sx = 800, u32 sy = 600) : size_x{sx}, size_y{sy} {
            glfwInit();
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfw_window_ptr = glfwCreateWindow(static_cast<i32>(size_x), static_cast<i32>(size_y), window_name, nullptr, nullptr);
        }

        ~Window() {
            glfwDestroyWindow(glfw_window_ptr);
            glfwTerminate();
        }

        void set_callbacks();
        auto get_native_handle() -> daxa::NativeWindowHandle;
        auto get_native_platform() -> daxa::NativeWindowPlatform;

        inline void set_mouse_pos(f32 x, f32 y) {
            glfwSetCursorPos(glfw_window_ptr, static_cast<f64>(x), static_cast<f64>(y));
        }

        inline void set_mouse_capture(bool should_capture) {
            glfwSetCursorPos(glfw_window_ptr, static_cast<f64>(size_x / 2), static_cast<f64>(size_y / 2));
            glfwSetInputMode(glfw_window_ptr, GLFW_CURSOR, should_capture ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            glfwSetInputMode(glfw_window_ptr, GLFW_RAW_MOUSE_MOTION, should_capture);
        }
    };
}