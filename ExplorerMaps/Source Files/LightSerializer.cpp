#include "LightSerializer.h"

#include <filesystem>
#include <fstream>
#include <iomanip>

#include <nlohmann/json.hpp>

namespace
{
    using json = nlohmann::json;

    LightVisualType LightVisualTypeFromInt(int value)
    {
        return value == static_cast<int>(LightVisualType::Cube)
            ? LightVisualType::Cube
            : LightVisualType::Point;
    }

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

bool LightSerializer::Load(const std::string& filePath, std::vector<Light>& lights) const
{
    std::ifstream input(filePath);
    if (!input.is_open())
    {
        return false;
    }

    json document;
    input >> document;
    if (!document.contains("lights") || !document["lights"].is_array())
    {
        return false;
    }

    lights.clear();
    for (const auto& lightJson : document["lights"])
    {
        Light light;
        light.id = lightJson.value("id", std::string("light"));
        light.name = lightJson.value("name", light.id);
        light.position = ToVec3(lightJson.value("position", json::array()), light.position);
        light.color = ToVec3(lightJson.value("color", json::array()), light.color);
        light.intensity = lightJson.value("intensity", light.intensity);
        light.radius = lightJson.value("radius", light.radius);
        light.helperSize = lightJson.value("helperSize", light.helperSize);
        light.visualType = LightVisualTypeFromInt(lightJson.value("visualType", static_cast<int>(light.visualType)));
        light.boxSize = ToVec3(lightJson.value("boxSize", json::array()), light.boxSize);
        light.selected = false;
        lights.push_back(light);
    }

    return true;
}

bool LightSerializer::Save(const std::string& filePath, const std::vector<Light>& lights) const
{
    namespace fs = std::filesystem;

    const fs::path target(filePath);
    if (target.has_parent_path())
    {
        fs::create_directories(target.parent_path());
    }

    nlohmann::json document;
    document["lights"] = nlohmann::json::array();

    for (const Light& light : lights)
    {
        document["lights"].push_back({
            { "id", light.id },
            { "name", light.name },
            { "position", ToJson(light.position) },
            { "color", ToJson(light.color) },
            { "intensity", light.intensity },
            { "radius", light.radius },
            { "helperSize", light.helperSize },
            { "visualType", static_cast<int>(light.visualType) },
            { "boxSize", ToJson(light.boxSize) }
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
