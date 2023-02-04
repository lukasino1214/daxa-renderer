#include "scene_serializer.hpp"

#include "entity.hpp"
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace YAML {
    template<>
    struct convert<glm::vec3> {
        static Node encode(const glm::vec3 &rhs) {
            Node node;
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);
            node.SetStyle(EmitterStyle::Flow);
            return node;
        }

        static bool decode(const Node &node, glm::vec3 &rhs) {
            if (!node.IsSequence() || node.size() != 3)
                return false;

            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();
            rhs.z = node[2].as<float>();
            return true;
        }
    };

    template<>
    struct convert<glm::vec4> {
        static Node encode(const glm::vec4 &rhs) {
            Node node;
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);
            node.push_back(rhs.w);
            node.SetStyle(EmitterStyle::Flow);
            return node;
        }

        static bool decode(const Node &node, glm::vec4 &rhs) {
            if (!node.IsSequence() || node.size() != 4)
                return false;

            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();
            rhs.z = node[2].as<float>();
            rhs.w = node[3].as<float>();
            return true;
        }
    };
}

YAML::Emitter &operator<<(YAML::Emitter &out, const glm::vec3 &v) {
    out << YAML::Flow;
    out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
    return out;
}

YAML::Emitter &operator<<(YAML::Emitter &out, const glm::vec4 &v) {
    out << YAML::Flow;
    out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
    return out;
}

namespace dare {
    static void serialize_entity(YAML::Emitter &out, Entity entity) {
        out << YAML::BeginMap;
        out << YAML::Key << "Entity" << YAML::Value << entity.get_component<IDComponent>().ID;

        if(entity.has_component<TagComponent>()) {
            out << YAML::Key << "TagComponent";
            out << YAML::BeginMap;

            out << YAML::Key << "Tag" << YAML::Value << entity.get_component<TagComponent>().tag;

            out << YAML::EndMap;
        }

        if(entity.has_component<TransformComponent>()) {
            out << YAML::Key << "TransformComponent";
            out << YAML::BeginMap;

            auto& comp = entity.get_component<TransformComponent>();
            out << YAML::Key << "Translation" << YAML::Value << comp.translation;
            out << YAML::Key << "Rotation" << YAML::Value << comp.rotation;
            out << YAML::Key << "Scale" << YAML::Value << comp.scale;

            out << YAML::EndMap;
        }

        if(entity.has_component<ModelComponent>()) {
            out << YAML::Key << "ModelComponent";
            out << YAML::BeginMap;

            out << YAML::Key << "Path" << YAML::Value << entity.get_component<ModelComponent>().model->path;

            out << YAML::EndMap;
        }

        if(entity.has_component<DirectionalLightComponent>()) {
            out << YAML::Key << "DirectionalLightComponent";
            out << YAML::BeginMap;

            out << YAML::Key << "Direction" << YAML::Value << entity.get_component<DirectionalLightComponent>().direction;
            out << YAML::Key << "Color" << YAML::Value << entity.get_component<DirectionalLightComponent>().color;
            out << YAML::Key << "Intensity" << YAML::Value << entity.get_component<DirectionalLightComponent>().intensity;

            out << YAML::EndMap;
        }

        if(entity.has_component<PointLightComponent>()) {
            out << YAML::Key << "PointLightComponent";
            out << YAML::BeginMap;

            out << YAML::Key << "Color" << YAML::Value << entity.get_component<PointLightComponent>().color;
            out << YAML::Key << "Intensity" << YAML::Value << entity.get_component<PointLightComponent>().intensity;

            out << YAML::EndMap;
        }

        if(entity.has_component<SpotLightComponent>()) {
            out << YAML::Key << "SpotLightComponent";
            out << YAML::BeginMap;

            out << YAML::Key << "Direction" << YAML::Value << entity.get_component<SpotLightComponent>().direction;
            out << YAML::Key << "Color" << YAML::Value << entity.get_component<SpotLightComponent>().color;
            out << YAML::Key << "Intensity" << YAML::Value << entity.get_component<SpotLightComponent>().intensity;
            out << YAML::Key << "CutOff" << YAML::Value << entity.get_component<SpotLightComponent>().cut_off;
            out << YAML::Key << "OuterCutOff" << YAML::Value << entity.get_component<SpotLightComponent>().outer_cut_off;

            out << YAML::EndMap;
        }

        out << YAML::EndMap;
    }

    void SceneSerializer::serialize(const std::shared_ptr<Scene> scene, const std::string& file_path) {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Scene" << YAML::Value << "Untitled";
        out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

        scene->iterate([&](Entity entity){
            serialize_entity(out, entity);
        });

        out << YAML::EndSeq;
        out << YAML::EndMap;

        std::ofstream fout(file_path);
        fout << out.c_str();
    }

    std::shared_ptr<Scene> SceneSerializer::deserialize(const std::shared_ptr<ModelManager>& model_manager, const std::string& file_path) {
        YAML::Node data;
        try {
            data = YAML::LoadFile(file_path);
        }
        catch (YAML::ParserException& e) {
            throw std::runtime_error("Couldn't load scene");
        }

        if (!data["Scene"])
            throw std::runtime_error("Couldn't load scene");

        auto scene = std::make_shared<Scene>(model_manager->device);

        auto scene_name = data["Scene"].as<std::string>();
        auto entities = data["Entities"];
        if(entities) {
            for(auto entity : entities) {
                auto uuid = entity["Entity"].as<u64>();
                std::string name;
                auto tag_component = entity["TagComponent"];
                if(tag_component) {
                    name = tag_component["Tag"].as<std::string>();
                }

                Entity deserialized_entity = scene->create_entity_with_UUID(uuid, name);

                auto transform_component = entity["TransformComponent"];
                if(transform_component) {
                    auto& comp = deserialized_entity.get_component<TransformComponent>();
                    comp.translation = transform_component["Translation"].as<glm::vec3>();
                    comp.rotation = transform_component["Rotation"].as<glm::vec3>();
                    comp.scale = transform_component["Scale"].as<glm::vec3>();
                }

                auto model_component = entity["ModelComponent"];
                if(model_component) {
                    //auto model = std::make_shared<Model>(device, model_component["Path"].as<std::string>());
                    std::shared_ptr<Model> model = model_manager->load_model(model_component["Path"].as<std::string>());
                    //model->model_manager = model_manager;
                    deserialized_entity.add_component<ModelComponent>(model);
                }

                auto directional_light_component = entity["DirectionalLightComponent"];
                if(directional_light_component) {
                    auto& light = deserialized_entity.add_component<DirectionalLightComponent>();
                    light.direction = directional_light_component["Direction"].as<glm::vec3>();
                    light.color = directional_light_component["Color"].as<glm::vec3>();
                    light.intensity = directional_light_component["Intensity"].as<f32>();

                    light.shadow_info.has_to_create = true;
                    light.shadow_info.has_moved = true;
                }

                auto point_light_component = entity["PointLightComponent"];
                if(point_light_component) {
                    auto& light = deserialized_entity.add_component<PointLightComponent>();
                    light.color = point_light_component["Color"].as<glm::vec3>();
                    light.intensity = point_light_component["Intensity"].as<f32>();

                    light.shadow_info.has_to_create = true;
                    light.shadow_info.has_moved = true;
                }

                auto spot_light_component = entity["SpotLightComponent"];
                if(spot_light_component) {
                    auto& light = deserialized_entity.add_component<SpotLightComponent>();
                    light.direction = spot_light_component["Direction"].as<glm::vec3>();
                    light.color = spot_light_component["Color"].as<glm::vec3>();
                    light.intensity = spot_light_component["Intensity"].as<f32>();
                    light.cut_off = spot_light_component["CutOff"].as<f32>();
                    light.outer_cut_off = spot_light_component["OuterCutOff"].as<f32>();

                    light.shadow_info.has_to_create = true;
                    light.shadow_info.has_moved = true;
                }
            }
        }

        return scene;
    }
}