#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "EditorTypes.h"

struct BoxCollider
{
    std::string id;
    std::string name;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(4.0f, 8.0f, 4.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    bool blocking = true;
    bool visible = true;
};

struct ColliderSelection
{
    int index = -1;

    bool IsValid() const
    {
        return index >= 0;
    }
};

inline glm::mat4 ComposeColliderMatrix(const BoxCollider& collider)
{
    return ComposeTransformMatrix(collider.position, collider.rotation, collider.scale);
}

inline glm::vec3 ColliderHalfExtents(const BoxCollider& collider)
{
    return glm::max(collider.scale * 0.5f, glm::vec3(0.05f));
}

