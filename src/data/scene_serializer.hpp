#pragma once

#include "scene.hpp"

#include <memory>
#include <daxa/daxa.hpp>

namespace dare {
    struct SceneSerializer {
        static void serialize(const std::shared_ptr<Scene> scene, const std::string& file_path);
        static std::shared_ptr<Scene> deserialize(daxa::Device& device, const std::string& file_path);
    };
}