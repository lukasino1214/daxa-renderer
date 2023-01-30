#pragma once

#include <string>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include "UUID.hpp"
#include "../graphics/model.hpp"
#include "../graphics/buffer.hpp"

namespace dare {
    struct IDComponent {
        UUID ID;

        IDComponent() = default;
        IDComponent(const IDComponent&) = default;
        IDComponent(const UUID &uuid) : ID(uuid) {}
    };

    struct TagComponent {
        std::string tag;

        TagComponent() = default;
        TagComponent(const TagComponent&) = default;
        TagComponent(const std::string &_tag) : tag(_tag) {}
    };

    struct TransformComponent {
        glm::vec3 translation = {0.001f, 0.001f, 0.001f};
        glm::vec3 rotation = {0.001f, 0.001f, 0.001f};
        glm::vec3 scale = {1.0f, 1.0f, 1.0f};
        glm::mat4 model_matrix = glm::mat4(1.0);
        glm::mat4 normal_matrix = glm::mat4(1.0);
        bool is_dirty = true;

        TransformComponent() = default;
        TransformComponent(const TransformComponent&) = default;
        TransformComponent(const glm::vec3 &_translation) : translation(_translation) {}

        void set_translation(const glm::vec3 &_translation) { translation = _translation; is_dirty = true; }
        void set_rotation(const glm::vec3 &_rotation) { rotation = _rotation; is_dirty = true; }
        void set_scale(const glm::vec3 &_scale) { scale = _scale; is_dirty = true; }

        auto calculate_matrix() const -> glm::mat4 {
            return glm::translate(glm::mat4(1.0f), translation) 
                * glm::toMat4(glm::quat({glm::radians(rotation.x), glm::radians(rotation.y), glm::radians(rotation.z)})) 
                * glm::scale(glm::mat4(1.0f), scale);
        }

        auto calculate_normal_matrix() const -> glm::mat4 {
            return glm::transpose(glm::inverse(calculate_matrix()));
        }

        std::shared_ptr<Buffer<ObjectInfo>> object_info;
    };

    struct ModelComponent {
        std::shared_ptr<Model> model{};

        ModelComponent() = default;
        ModelComponent(const ModelComponent&) = default;
        ModelComponent(const std::shared_ptr<Model> &_model) : model{_model} {};
    };

    enum struct ShadowType: i32 {
        NONE = 0,
        PCF = 1,
        VARIANCE = 2
    };

    struct ShadowInfo {
        glm::mat4 view;
        glm::mat4 projection;
        daxa::ImageId depth_image;
        daxa::ImageId shadow_image;
        daxa::ImageId temp_shadow_image;
        bool has_to_create = true;
        bool has_moved = true;
        bool has_to_resize = false;
        glm::ivec2 image_size = {1024, 1024};
        f32 clip_space = 128.0f;
        i32 update_time = 250;
        ShadowType type = ShadowType::NONE;
    };

    struct DirectionalLightComponent {
        glm::vec3 direction = { 0.0f, -1.0f, 0.0f };
        glm::vec3 color = { 1.0f, 1.0f, 1.0f };
        f32 intensity = 32.0f;
        ShadowInfo shadow_info;

        DirectionalLightComponent() = default;
        DirectionalLightComponent(const DirectionalLightComponent&) = default;
    };

    struct PointLightComponent {
        glm::vec3 color = { 1.0f, 1.0f, 1.0f };
        f32 intensity = 32.0f;
        ShadowInfo shadow_info;

        PointLightComponent() = default;
        PointLightComponent(const PointLightComponent&) = default;
    };

    struct SpotLightComponent {
        glm::vec3 direction = { 0.0f, -1.0f, 0.0f };
        glm::vec3 color = { 1.0f, 1.0f, 1.0f };
        f32 intensity = 32.0f;
        f32 cut_off = 20.0f;
        f32 outer_cut_off = 30.0f;
        ShadowInfo shadow_info;

        SpotLightComponent() = default;
        SpotLightComponent(const SpotLightComponent&) = default;
    };
}