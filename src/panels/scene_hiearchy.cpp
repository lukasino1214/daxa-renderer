#include "scene_hiearchy.hpp"

#include <imgui.h>
#include "../utils/utils.hpp"

namespace dare {
    SceneHiearchyPanel::SceneHiearchyPanel(std::shared_ptr<Scene> scene) : scene{scene} {

    }

    SceneHiearchyPanel::~SceneHiearchyPanel() {

    }

    template<typename T>
    static inline void draw_component(std::string_view name, Entity entity, std::function<void(T&)> fn) {
        if(entity.has_component<T>()) {
            ImGui::Text("%s", name.data());
            ImGui::Separator();
            fn(entity.get_component<T>());
            ImGui::Separator();
        }
    }

    void SceneHiearchyPanel::draw() {
        ImGui::Begin("Scene Hiearchy");

        scene->iterate([=](Entity entity) {
            auto& name = entity.get_component<TagComponent>().tag;
            if(ImGui::Button(name.c_str())) {
                selected_entity = entity;
            }
        });

        ImGui::End();
        ImGui::Begin("Entity Properties");
        if(selected_entity) {
            draw_component<TagComponent>("TagComponent", selected_entity, [](TagComponent& comp) {
                ImGui::Text("Entity name: %s", comp.tag.c_str());
            });

            draw_component<TransformComponent>("TransformComponent", selected_entity, [](TransformComponent& comp) {
                glm::vec3 translation = comp.translation;
                glm::vec3 rotation = comp.rotation;
                glm::vec3 scale = comp.scale;

                ImGui::DragFloat3("Translation: ", &comp.translation.x);
                ImGui::DragFloat3("Rotation: ", &comp.rotation.x);
                ImGui::DragFloat3("Scale: ", &comp.scale.x);

                if(translation != comp.translation) { comp.is_dirty = true; }
                if(rotation != comp.rotation) { comp.is_dirty = true; }
                if(scale != comp.scale) { comp.is_dirty = true; }
            });

            draw_component<ModelComponent>("ModelComponent", selected_entity, [](ModelComponent& comp) {
                ImGui::Text("File path: %s", comp.model->path.c_str());
            });

            draw_component<LightComponent>("LightComponent", selected_entity, [](LightComponent& comp) {
                ImGui::ColorPicker3("Color", &comp.color.x);
                ImGui::DragFloat("Intensity", &comp.intensity);
            });

        } else {
            ImGui::Text("Invalid Entity");
        }
        ImGui::End();
    }
}