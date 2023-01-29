#pragma once
#include <entt/entt.hpp>

#include "UUID.hpp"
#include "../graphics/buffer.hpp"

using namespace daxa::types;
#include "../../shaders/shared.inl"

namespace dare {
    struct Entity;
    struct Scene {
        public:
            Scene(daxa::Device& device);
            ~Scene();

            Entity create_entity(const std::string &name = std::string());
            Entity create_entity_with_UUID(UUID uuid, const std::string &name = std::string());
            void destroy_entity(Entity entity);
            void iterate(std::function<void(Entity)> fn);
            void update(daxa::CommandList& cmd_list);

            std::unique_ptr<Buffer<LightsInfo>> lights_buffer;

            daxa::Device& device;
        private:
            entt::registry registry;
            friend Entity;
    };
}