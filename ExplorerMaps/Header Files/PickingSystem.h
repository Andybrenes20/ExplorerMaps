#pragma once

#include <glm/glm.hpp>

#include "EditorTypes.h"

struct PickingViewport
{
    glm::vec2 mousePosition = glm::vec2(0.0f);
    glm::vec2 viewportSize = glm::vec2(1.0f);
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);
    glm::vec3 cameraPosition = glm::vec3(0.0f);
};

class PickingSystem
{
public:
    SceneSelection Pick(const EditorSceneData& scene, const PickingViewport& viewport) const;
};
