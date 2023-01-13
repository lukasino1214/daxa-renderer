#pragma once

#include <memory>
#include "../data/scene.hpp"
#include "../data/entity.hpp"

namespace dare {
    struct SceneHiearchyPanel {
        std::shared_ptr<Scene> scene;
        Entity selected_entity;

        SceneHiearchyPanel(std::shared_ptr<Scene> scene);
        ~SceneHiearchyPanel();

        void draw();
    };
}