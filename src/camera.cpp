#include "camera.hpp"

void Camera3D::resize(i32 size_x, i32 size_y) {
    aspect = static_cast<f32>(size_x) / static_cast<f32>(size_y);
    proj_mat = glm::perspective(glm::radians(fov), aspect, near_clip, far_clip);
    //proj_mat[1][1] *= -1.0f;
}

void Camera3D::set_pos(glm::vec3 pos) {
    vtrn_mat = glm::translate(glm::mat4(1), glm::vec3(pos.x, pos.y, pos.z));
}

void Camera3D::set_rot(f32 x, f32 y) {
    vrot_mat = glm::rotate(glm::rotate(glm::mat4(1), y, {1, 0, 0}), x, {0, 1, 0});
}

auto Camera3D::get_vp() -> glm::mat4 {
    return proj_mat * vrot_mat * vtrn_mat;
}

auto Camera3D::get_view() -> glm::mat4 {
    return vrot_mat * vtrn_mat;
}

void ControlledCamera3D::update(f32 dt) {
    auto delta_pos = speed * dt;
    if (move.sprint)
        delta_pos *= sprint_speed;
    if (move.px)
        pos.z += sin_rot_x * delta_pos, pos.x += cos_rot_x * delta_pos;
    if (move.nx)
        pos.z -= sin_rot_x * delta_pos, pos.x -= cos_rot_x * delta_pos;
    if (move.pz)
        pos.x -= sin_rot_x * delta_pos, pos.z += cos_rot_x * delta_pos;
    if (move.nz)
        pos.x += sin_rot_x * delta_pos, pos.z -= cos_rot_x * delta_pos;
    if (move.py)
        pos.y += delta_pos;
    if (move.ny)
        pos.y -= delta_pos;

    constexpr auto MAX_ROT = std::numbers::pi_v<f32> / 2;
    if (rot.y > MAX_ROT)
        rot.y = MAX_ROT;
    if (rot.y < -MAX_ROT)
        rot.y = -MAX_ROT;
}

void ControlledCamera3D::on_key(i32 key, i32 action) {
    if (key == keybinds.move_pz)
        move.pz = action != 0;
    if (key == keybinds.move_nz)
        move.nz = action != 0;
    if (key == keybinds.move_px)
        move.px = action != 0;
    if (key == keybinds.move_nx)
        move.nx = action != 0;
    if (key == keybinds.move_py)
        move.py = action != 0;
    if (key == keybinds.move_ny)
        move.ny = action != 0;
    if (key == keybinds.toggle_sprint)
        move.sprint = action != 0;
}

void ControlledCamera3D::on_mouse_move(f32 delta_x, f32 delta_y) {
    rot.x += delta_x * mouse_sens * 0.0001f * camera.fov;
    rot.y += delta_y * mouse_sens * 0.0001f * camera.fov;
    sin_rot_x = std::sin(rot.x);
    cos_rot_x = std::cos(rot.x);
}