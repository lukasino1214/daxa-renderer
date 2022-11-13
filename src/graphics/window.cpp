#include "window.hpp"

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_NATIVE_INCLUDE_NONE
using HWND = void *;
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#endif
#include <GLFW/glfw3native.h>

namespace dare {
    auto Window::get_native_handle() -> daxa::NativeWindowHandle {
    #if defined(_WIN32)
        return glfwGetWin32Window(glfw_window_ptr);
    #elif defined(__linux__)
        switch (get_native_platform()) {
            case daxa::NativeWindowPlatform::WAYLAND_API:
                return reinterpret_cast<daxa::NativeWindowHandle>(glfwGetWaylandWindow(glfw_window_ptr));
            case daxa::NativeWindowPlatform::XLIB_API:
            default:
                return reinterpret_cast<daxa::NativeWindowHandle>(glfwGetX11Window(glfw_window_ptr));
        }
    #endif
    }

    auto Window::get_native_platform() -> daxa::NativeWindowPlatform {
        switch(glfwGetPlatform()) {
            case GLFW_PLATFORM_WIN32: return daxa::NativeWindowPlatform::WIN32_API;
            case GLFW_PLATFORM_X11: return daxa::NativeWindowPlatform::XLIB_API;
            case GLFW_PLATFORM_WAYLAND: return daxa::NativeWindowPlatform::WAYLAND_API;
            default: return daxa::NativeWindowPlatform::UNKNOWN;
        }
    }
}