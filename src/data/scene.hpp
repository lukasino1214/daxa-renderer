#pragma once
#include <entt/entt.hpp>

#include "UUID.hpp"

class Entity;
class Scene {
    public:
         Scene();
        ~Scene();

        Entity create_entity(const std::string &name = std::string());
        Entity create_entity_with_UUID(UUID uuid, const std::string &name = std::string());
        void destroy_entity(Entity entity);

    private:
        entt::registry registry;

        friend Entity;
};