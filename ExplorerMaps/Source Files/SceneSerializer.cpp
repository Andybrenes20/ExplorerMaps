#include "SceneSerializer.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>

#include <nlohmann/json.hpp>

namespace
{
    using json = nlohmann::json;

    json ToJson(const glm::vec3& value)
    {
        return json::array({ value.x, value.y, value.z });
    }

    glm::vec3 ToVec3(const json& value, const glm::vec3& fallback)
    {
        if (!value.is_array() || value.size() != 3)
        {
            return fallback;
        }

        return glm::vec3(
            value.at(0).get<float>(),
            value.at(1).get<float>(),
            value.at(2).get<float>());
    }
}

bool SceneSerializer::Load(const std::string& filePath, EditorSceneData& scene) const
{
    std::ifstream input(filePath);
    if (!input.is_open())
    {
        return false;
    }

    json document;
    input >> document;

    if (document.contains("entities") && document["entities"].is_array())
    {
        for (const auto& entityJson : document["entities"])
        {
            const std::string id = entityJson.value("id", std::string());
            auto entityIt = std::find_if(scene.entities.begin(), scene.entities.end(), [&](const Entity& entity)
                {
                    return entity.id == id;
                });

            if (entityIt == scene.entities.end())
            {
                continue;
            }

            entityIt->assetPath = entityJson.value("assetPath", entityIt->assetPath);
            entityIt->position = ToVec3(entityJson.value("position", json::array()), entityIt->position);
            entityIt->rotation = ToVec3(entityJson.value("rotation", json::array()), entityIt->rotation);
            entityIt->scale = ToVec3(entityJson.value("scale", json::array()), entityIt->scale);
        }
    }

    if (document.contains("helpers") && document["helpers"].is_array())
    {
        scene.helpers.clear();
        for (const auto& helperJson : document["helpers"])
        {
            Helper helper;
            helper.id = helperJson.value("id", std::string("helper"));
            helper.name = helperJson.value("name", helper.id);
            helper.position = ToVec3(helperJson.value("position", json::array()), helper.position);
            helper.rotation = ToVec3(helperJson.value("rotation", json::array()), helper.rotation);
            helper.scale = ToVec3(helperJson.value("scale", json::array()), helper.scale);
            helper.color = ToVec3(helperJson.value("color", json::array()), helper.color);
            scene.helpers.push_back(helper);
        }
    }

    return true;
}

bool SceneSerializer::Save(const std::string& filePath, const EditorSceneData& scene) const
{
    namespace fs = std::filesystem;

    const fs::path target(filePath);
    if (target.has_parent_path())
    {
        fs::create_directories(target.parent_path());
    }

    json document;
    document["entities"] = json::array();
    document["helpers"] = json::array();

    for (const Entity& entity : scene.entities)
    {
        document["entities"].push_back({
            { "id", entity.id },
            { "name", entity.name },
            { "assetPath", entity.assetPath },
            { "position", ToJson(entity.position) },
            { "rotation", ToJson(entity.rotation) },
            { "scale", ToJson(entity.scale) }
            });
    }

    for (const Helper& helper : scene.helpers)
    {
        document["helpers"].push_back({
            { "id", helper.id },
            { "name", helper.name },
            { "position", ToJson(helper.position) },
            { "rotation", ToJson(helper.rotation) },
            { "scale", ToJson(helper.scale) },
            { "color", ToJson(helper.color) }
            });
    }

    std::ofstream output(filePath);
    if (!output.is_open())
    {
        return false;
    }

    output << std::setw(2) << document;
    return true;
}
