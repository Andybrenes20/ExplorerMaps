#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Entity
{
    std::string id;
    std::string name;
    std::string assetPath;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    float pickRadius = 1.0f;
};

enum class LightVisualType
{
    Point = 0,
    Cube = 1
};

struct Light
{
    std::string id;
    std::string name;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;
    float radius = 10.0f;
    float helperSize = 8.0f;
    LightVisualType visualType = LightVisualType::Point;
    glm::vec3 boxSize = glm::vec3(8.0f);
    bool selected = false;
};

struct Helper
{
    std::string id;
    std::string name;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(10.0f);
    glm::vec3 color = glm::vec3(0.35f, 0.85f, 1.0f);
};

enum class SceneObjectType
{
    None,
    Entity,
    Light,
    Helper
};

struct SceneSelection
{
    SceneObjectType type = SceneObjectType::None;
    int index = -1;

    bool IsValid() const
    {
        return type != SceneObjectType::None && index >= 0;
    }
};

struct EditorSceneData
{
    std::vector<Entity> entities;
    std::vector<Light> lights;
    std::vector<Helper> helpers;
};

inline glm::mat4 ComposeTransformMatrix(const glm::vec3& position, const glm::vec3& rotationDegrees, const glm::vec3& scale)
{
    glm::mat4 matrix(1.0f);
    matrix = glm::translate(matrix, position);
    matrix = glm::rotate(matrix, glm::radians(rotationDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
    matrix = glm::rotate(matrix, glm::radians(rotationDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
    matrix = glm::rotate(matrix, glm::radians(rotationDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
    matrix = glm::scale(matrix, scale);
    return matrix;
}

inline glm::mat4 ComposeEntityMatrix(const Entity& entity)
{
    return ComposeTransformMatrix(entity.position, entity.rotation, entity.scale);
}

inline glm::mat4 ComposeHelperMatrix(const Helper& helper)
{
    return ComposeTransformMatrix(helper.position, helper.rotation, helper.scale);
}

inline float MaxComponent(const glm::vec3& value)
{
    return std::max(value.x, std::max(value.y, value.z));
}

inline glm::mat4 ComposeLightMatrix(const Light& light)
{
    const float uniformScale = light.visualType == LightVisualType::Cube
        ? std::max(MaxComponent(light.boxSize), 1.0f)
        : std::max(light.radius, 1.0f);
    return ComposeTransformMatrix(light.position, glm::vec3(0.0f), glm::vec3(uniformScale));
}
