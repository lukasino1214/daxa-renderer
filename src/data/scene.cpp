#include "scene.hpp"
#include "entity.hpp"
#include "components.hpp"

namespace dare {
    Scene::Scene(daxa::Device& device) : device{device} {}
    Scene::~Scene() = default;

    Entity Scene::create_entity(const std::string &name) {
        return create_entity_with_UUID(UUID(), name);
    }

    Entity Scene::create_entity_with_UUID(UUID uuid, const std::string &name) {
        Entity entity = {registry.create(), this};
        entity.add_component<IDComponent>(uuid);
        entity.add_component<TransformComponent>();
        entity.get_component<TransformComponent>().object_info = std::make_shared<Buffer<ObjectInfo>>(device);
        //entity.add_component<RelationshipComponent>();
        auto &tag = entity.add_component<TagComponent>();
        tag.tag = name.empty() ? "Entity" : name;
        return entity;
    }

    void Scene::destroy_entity(Entity entity) {
        registry.destroy(entity);
    }

    void Scene::iterate(std::function<void(Entity)> fn) {
        registry.each([&](auto entityID) {
            Entity entity = {entityID, this};
            if (!entity)
                return;

            fn(entity);
        });
    };

    void Scene::update(daxa::CommandList& cmd_list) {
        iterate([&](Entity entity) {
            auto& comp = entity.get_component<TransformComponent>();
            if(comp.is_dirty) {
                auto m = comp.calculate_matrix();
                auto n = comp.calculate_normal_matrix();
                comp.object_info->update(cmd_list, ObjectInfo {
                    .model_matrix = *reinterpret_cast<const f32mat4x4 *>(&m),
                    .normal_matrix = *reinterpret_cast<const f32mat4x4 *>(&n)
                });

                comp.is_dirty = false;
            }
        });
    }
}