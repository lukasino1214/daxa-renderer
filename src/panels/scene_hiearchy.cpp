#include "scene_hiearchy.hpp"

#include <imgui.h>

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

        scene->iterate([this](Entity entity) {
            auto& name = entity.get_component<TagComponent>().tag;
            if(ImGui::Button(name.c_str())) {
                selected_entity = entity;
            }

            bool entity_deleted = false;
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Delete Entity")) {
                    entity_deleted = true;
                }

                ImGui::EndPopup();
            }

            if(entity_deleted) {
                scene->destroy_entity(entity);
                if (selected_entity == entity)
                    selected_entity = {};
            }
        });

        if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
                selected_entity = {};

        // Right-click on blank space
        if (ImGui::BeginPopupContextWindow(nullptr, 1)) {
            if (ImGui::MenuItem("Create Empty Entity"))
                scene->create_entity("Empty Entity");

            ImGui::EndPopup();
        }

        ImGui::End();
        ImGui::Begin("Entity Properties");
        if(selected_entity) {
            if (ImGui::Button("Add Component"))
            ImGui::OpenPopup("AddComponent");

        if (ImGui::BeginPopup("AddComponent")) {
            if (ImGui::MenuItem("Directional Light")) {
                if (!selected_entity.has_component<DirectionalLightComponent>())
                    selected_entity.add_component<DirectionalLightComponent>();
                else
                    std::cout << "screw this" << std::endl;
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::MenuItem("Point Light")) {
                if (!selected_entity.has_component<PointLightComponent>())
                    selected_entity.add_component<PointLightComponent>();
                else
                    std::cout << "screw this" << std::endl;
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::MenuItem("Spot Light")) {
                if (!selected_entity.has_component<SpotLightComponent>())
                    selected_entity.add_component<SpotLightComponent>();
                else
                    std::cout << "screw this" << std::endl;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

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

            draw_component<DirectionalLightComponent>("DirectionalLightComponent", selected_entity, [](DirectionalLightComponent& comp) {
                ImGui::DragFloat3("Direction", &comp.direction.x);
                ImGui::ColorPicker3("Color", &comp.color.x);
                ImGui::DragFloat("Intensity", &comp.intensity);
            });

            draw_component<PointLightComponent>("PointLightComponent", selected_entity, [](PointLightComponent& comp) {
                ImGui::ColorPicker3("Color", &comp.color.x);
                ImGui::DragFloat("Intensity", &comp.intensity);
            });

            draw_component<SpotLightComponent>("SpotLightComponent", selected_entity, [](SpotLightComponent& comp) {
                ImGui::DragFloat3("Direction", &comp.direction.x);
                ImGui::ColorPicker3("Color", &comp.color.x);
                ImGui::DragFloat("Intensity", &comp.intensity);
                ImGui::DragFloat("CutOff", &comp.cut_off);
                ImGui::DragFloat("OuterCutOff", &comp.outer_cut_off);
            });

        } else {
            ImGui::Text("Invalid Entity");
        }
        ImGui::End();
    }
}