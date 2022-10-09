#include "scene.hpp"
#include "entity.hpp"
#include "components.hpp"

Scene::Scene() = default;
Scene::~Scene() = default;

Entity Scene::create_entity(const std::string &name) {
    return create_entity_with_UUID(UUID(), name);
}

Entity Scene::create_entity_with_UUID(UUID uuid, const std::string &name) {
    Entity entity = {registry.create(), this};
    entity.add_component<IDComponent>(uuid);
    entity.add_component<TransformComponent>();
    //entity.add_component<RelationshipComponent>();
    auto &tag = entity.add_component<TagComponent>();
    tag.tag = name.empty() ? "Entity" : name;
    return entity;
}

void Scene::destroy_entity(Entity entity) {
    registry.destroy(entity);
}