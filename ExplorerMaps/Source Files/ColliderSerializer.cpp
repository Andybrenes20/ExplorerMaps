#include "ColliderSerializer.h"

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

bool ColliderSerializer::Load(const std::string& filePath, ColliderManager& manager) const
{
    std::ifstream input(filePath);
    if (!input.is_open())
    {
        return false;
    }

    json document;
    input >> document;

    std::vector<BoxCollider> colliders;
    if (document.contains("colliders") && document["colliders"].is_array())
    {
        for (const auto& colliderJson : document["colliders"])
        {
            BoxCollider collider;
            collider.id = colliderJson.value("id", std::string());
            collider.name = colliderJson.value("name", collider.id.empty() ? std::string("Collider") : collider.id);
            collider.position = ToVec3(colliderJson.value("position", json::array()), collider.position);
            collider.scale = ToVec3(colliderJson.value("scale", json::array()), collider.scale);
            collider.rotation = ToVec3(colliderJson.value("rotation", json::array()), collider.rotation);
            collider.blocking = colliderJson.value("blocking", true);
            collider.visible = colliderJson.value("visible", true);
            colliders.push_back(collider);
        }
    }

    manager.SetColliders(colliders);
    return true;
}

bool ColliderSerializer::Save(const std::string& filePath, const ColliderManager& manager) const
{
    namespace fs = std::filesystem;

    const fs::path target(filePath);
    if (target.has_parent_path())
    {
        fs::create_directories(target.parent_path());
    }

    json document;
    document["colliders"] = json::array();

    for (const BoxCollider& collider : manager.GetColliders())
    {
        document["colliders"].push_back({
            { "id", collider.id },
            { "name", collider.name },
            { "position", ToJson(collider.position) },
            { "scale", ToJson(collider.scale) },
            { "rotation", ToJson(collider.rotation) },
            { "blocking", collider.blocking },
            { "visible", collider.visible }
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

