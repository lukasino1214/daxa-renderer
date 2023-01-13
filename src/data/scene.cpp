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

    void Scene::update() {
        auto cmd_list = device.create_command_list({
            .debug_name = "updating object info of entities",
        });

        LightsInfo info;
        info.num_directional_lights = 0;
        info.num_point_lights = 0;
        info.num_spot_lights = 0;

        iterate([&](Entity entity) {
            if(entity.has_component<DirectionalLightComponent>()) {
                auto& comp = entity.get_component<DirectionalLightComponent>();
                glm::vec3 dir = comp.direction;
                glm::vec3 col = comp.color;
                info.directional_lights[info.num_directional_lights].direction = *reinterpret_cast<const f32vec3 *>(&dir);
                info.directional_lights[info.num_directional_lights].color = *reinterpret_cast<const f32vec3 *>(&col);
                info.directional_lights[info.num_directional_lights].intensity = comp.intensity;
                info.num_directional_lights++;
                return;
            }

            if(entity.has_component<PointLightComponent>()) {
                auto& comp = entity.get_component<PointLightComponent>();
                glm::vec3 pos = entity.get_component<TransformComponent>().translation;
                glm::vec3 col = comp.color;
                info.point_lights[info.num_point_lights].position = *reinterpret_cast<const f32vec3 *>(&pos);
                info.point_lights[info.num_point_lights].color = *reinterpret_cast<const f32vec3 *>(&col);
                info.point_lights[info.num_point_lights].intensity = comp.intensity;
                info.num_point_lights++;
                return;
            }

            if(entity.has_component<SpotLightComponent>()) {
                auto& comp = entity.get_component<SpotLightComponent>();
                glm::vec3 pos = entity.get_component<TransformComponent>().translation;
                glm::vec3 dir = comp.direction;
                glm::vec3 col = comp.color;
                info.spot_lights[info.num_spot_lights].position = *reinterpret_cast<const f32vec3 *>(&pos);
                info.spot_lights[info.num_spot_lights].direction = *reinterpret_cast<const f32vec3 *>(&dir);
                info.spot_lights[info.num_spot_lights].color = *reinterpret_cast<const f32vec3 *>(&col);
                info.spot_lights[info.num_spot_lights].intensity = comp.intensity;
                info.spot_lights[info.num_spot_lights].cut_off = glm::cos(glm::radians(comp.cut_off));
                info.spot_lights[info.num_spot_lights].outer_cut_off = glm::cos(glm::radians(comp.outer_cut_off));
                info.num_spot_lights++;
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