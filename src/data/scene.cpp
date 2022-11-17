#include "scene.hpp"
#include "entity.hpp"
#include "components.hpp"

namespace dare {
    Scene::Scene(daxa::Device& device) : device{device} {
        lights_buffer = std::make_unique<Buffer<LightsInfo>>(device);
    }
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

    void Scene::iterate(std::function<void(Entity)> fn) {
        registry.each([&](auto entityID) {
            Entity entity = {entityID, this};
            if (!entity)
                return;

            fn(entity);
        });
    };

    void Scene::update() {
        auto cmd_list = device.create_command_list({
            .debug_name = APPNAME_PREFIX("updating object info of entities"),
        });

        LightsInfo info;
        info.num_point_lights = 0;

        iterate([&](Entity entity) {
            if(entity.has_component<LightComponent>()) {
                auto& comp = entity.get_component<LightComponent>();
                glm::vec3 pos = entity.get_component<TransformComponent>().translation;
                glm::vec3 col = comp.color;
                info.point_lights[info.num_point_lights].position = *reinterpret_cast<const f32vec3 *>(&pos);
                info.point_lights[info.num_point_lights].color = *reinterpret_cast<const f32vec3 *>(&col);
                info.point_lights[info.num_point_lights].intensity = comp.intensity;
                info.num_point_lights++;
                return;
            }

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

        lights_buffer->update(cmd_list, info);

        cmd_list.complete();
        device.submit_commands({
            .command_lists = {std::move(cmd_list)}
        });
    }
}