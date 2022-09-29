#pragma once

#include <daxa/types.hpp>

#define GLM_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

using namespace daxa::types;

struct Camera3D {
    f32 fov = 98.6f, aspect = 1.0f;
    f32 near_clip = 0.01f, far_clip = 1000.0f;
    glm::mat4 proj_mat{};
    glm::mat4 vtrn_mat{};
    glm::mat4 vrot_mat{};

    void resize(i32 size_x, i32 size_y) {
        aspect = static_cast<f32>(size_x) / static_cast<f32>(size_y);
        proj_mat = glm::perspective(glm::radians(fov), aspect, near_clip, far_clip);
    }

    void set_pos(glm::vec3 pos) {
        vtrn_mat = glm::translate(glm::mat4(1), glm::vec3(pos.x, pos.y, pos.z));
    }

    void set_rot(f32 x, f32 y) {
        vrot_mat = glm::rotate(glm::rotate(glm::mat4(1), y, {1, 0, 0}), x, {0, 1, 0});
    }

    glm::mat4 get_vp() {
        return proj_mat * vrot_mat * vtrn_mat;
    }

    glm::mat4 get_view() {
        return vrot_mat * vtrn_mat;
    }
};

namespace input {
    struct Keybinds {
        i32 move_pz, move_nz;
        i32 move_px, move_nx;
        i32 move_py, move_ny;
        i32 toggle_pause;
        i32 toggle_sprint;
    };

    static inline constexpr Keybinds DEFAULT_KEYBINDS {
        .move_pz = GLFW_KEY_W,
        .move_nz = GLFW_KEY_S,
        .move_px = GLFW_KEY_A,
        .move_nx = GLFW_KEY_D,
        .move_py = GLFW_KEY_SPACE,
        .move_ny = GLFW_KEY_LEFT_CONTROL,
        .toggle_pause = GLFW_KEY_ESCAPE,
        .toggle_sprint = GLFW_KEY_LEFT_SHIFT,
    };
} // namespace input

struct ControlledCamera3D {
    Camera3D camera{};
    input::Keybinds keybinds = input::DEFAULT_KEYBINDS;
    glm::vec3 pos{0, 0, 0}, vel{}, rot{};
    f32 speed = 30.0f, mouse_sens = 0.1f;
    f32 sprint_speed = 8.0f;
    f32 sin_rot_x = 0, cos_rot_x = 1;

    struct MoveFlags {
        uint8_t px : 1, py : 1, pz : 1, nx : 1, ny : 1, nz : 1, sprint : 1;
    } move{};

    void update(f32 dt);
    void on_key(i32 key, i32 action);
    void on_mouse_move(f32 delta_x, f32 delta_y);
};