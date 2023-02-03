#pragma once

#include "scene.hpp"

#include <memory>
#include <daxa/daxa.hpp>

#include "../manager/model_manager.hpp"

namespace dare {
    struct SceneSerializer {
        static void serialize(const std::shared_ptr<Scene> scene, const std::string& file_path);
        static std::shared_ptr<Scene> deserialize(const std::shared_ptr<ModelManager>& model_manager, const std::string& file_path);
    };
}