#pragma once
#include <entt/entt.hpp>

#include "UUID.hpp"

namespace dare {
    struct Entity;
    struct Scene {
        public:
            Scene();
            ~Scene();

            Entity create_entity(const std::string &name = std::string());
            Entity create_entity_with_UUID(UUID uuid, const std::string &name = std::string());
            void destroy_entity(Entity entity);
            void iterate(std::function<void(Entity)> fn);
        private:
            entt::registry registry;
            friend Entity;
    };
}