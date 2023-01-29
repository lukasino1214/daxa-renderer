#include "scene_hiearchy.hpp"

#include <imgui.h>
#include <iostream>

namespace dare {
    SceneHiearchyPanel::SceneHiearchyPanel(std::shared_ptr<Scene> scene) : scene{scene} {

    }

    SceneHiearchyPanel::~SceneHiearchyPanel() {

    }

    template<typename T>
    static inline void draw_component(std::string_view name, Entity entity, std::function<void(Entity&, T&)> fn) {
        if(entity.has_component<T>()) {
            ImGui::Text("%s", name.data());
            ImGui::Separator();
            fn(entity, entity.get_component<T>());
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

            draw_component<TagComponent>("TagComponent", selected_entity, [](Entity entity, TagComponent& comp) {
                ImGui::Text("Entity name: %s", comp.tag.c_str());
            });

            draw_component<TransformComponent>("TransformComponent", selected_entity, [](Entity entity, TransformComponent& comp) {
                glm::vec3 translation = comp.translation;
                glm::vec3 rotation = comp.rotation;
                glm::vec3 scale = comp.scale;

                ImGui::DragFloat3("Translation: ", &comp.translation.x);
                ImGui::DragFloat3("Rotation: ", &comp.rotation.x);
                ImGui::DragFloat3("Scale: ", &comp.scale.x);

                if(translation != comp.translation) { comp.is_dirty = true; }
                if(rotation != comp.rotation) { comp.is_dirty = true; }
                if(scale != comp.scale) { comp.is_dirty = true; }

                if(comp.is_dirty) {
                    if(entity.has_component<DirectionalLightComponent>()) {
                        auto& light = entity.get_component<DirectionalLightComponent>();
                        light.shadow_info.has_moved = true;
                    }

                    if(entity.has_component<PointLightComponent>()) {
                        auto& light = entity.get_component<PointLightComponent>();
                        light.shadow_info.has_moved = true;
                    }

                    if(entity.has_component<SpotLightComponent>()) {
                        auto& light = entity.get_component<SpotLightComponent>();
                        light.shadow_info.has_moved = true;
                    }
                }
            });

            draw_component<ModelComponent>("ModelComponent", selected_entity, [](Entity entity, ModelComponent& comp) {
                ImGui::Text("File path: %s", comp.model->path.c_str());
            });

            draw_component<DirectionalLightComponent>("DirectionalLightComponent", selected_entity, [](Entity entity, DirectionalLightComponent& comp) {
                ImGui::DragFloat3("Direction", &comp.direction.x);
                ImGui::ColorPicker3("Color", &comp.color.x);
                ImGui::DragFloat("Intensity", &comp.intensity);
                ImGui::DragInt("Shadow Update Time", &comp.shadow_info.update_time, 1, 1, 1000);
            });

            draw_component<PointLightComponent>("PointLightComponent", selected_entity, [](Entity entity, PointLightComponent& comp) {
                ImGui::ColorPicker3("Color", &comp.color.x);
                ImGui::DragFloat("Intensity", &comp.intensity);
                ImGui::DragInt("Shadow Update Time", &comp.shadow_info.update_time, 1, 1, 1000);
            });

            draw_component<SpotLightComponent>("SpotLightComponent", selected_entity, [](Entity entity, SpotLightComponent& comp) {
                ImGui::DragFloat3("Direction", &comp.direction.x);
                ImGui::ColorPicker3("Color", &comp.color.x);
                ImGui::DragFloat("Intensity", &comp.intensity);
                ImGui::DragFloat("CutOff", &comp.cut_off);
                ImGui::DragFloat("OuterCutOff", &comp.outer_cut_off);
                ImGui::DragInt("Shadow Update Time", &comp.shadow_info.update_time, 1, 1, 1000);
            });

        } else {
            ImGui::Text("Invalid Entity");
        }
        ImGui::End();
    }
}