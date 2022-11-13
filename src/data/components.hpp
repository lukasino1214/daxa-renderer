#pragma once

#include <string>
#include <memory>

#include <glm/glm.hpp>
#include "UUID.hpp"
#include "../graphics/model.hpp"

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
        glm::vec3 translation = {0.0f, 0.0f, 0.0f};
        glm::vec3 rotation = {0.0f, 0.0f, 0.0f};
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
    };

    struct ModelComponent {
        std::shared_ptr<Model> model{};

        ModelComponent() = default;
        ModelComponent(const ModelComponent&) = default;
        ModelComponent(const std::shared_ptr<Model> &_model) : model{_model} {};
    };
}