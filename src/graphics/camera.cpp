#include "camera.hpp"

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