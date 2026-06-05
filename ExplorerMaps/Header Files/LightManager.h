#pragma once

#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "EditorTypes.h"

class LightManager
{
public:
    void Bind(std::vector<Light>* lights);
    void SyncSelection(const SceneSelection& selection);
    void HandleShortcuts(GLFWwindow* window, const glm::vec3& spawnPosition, SceneSelection& selection, bool& dirty);

    int CreatePointLight(const glm::vec3& position);
    int CreateAreaLight(const glm::vec3& position);
    int CreateCubeLight(const glm::vec3& position);
    int DuplicateSelected(const SceneSelection& selection);
    bool DeleteSelected(SceneSelection& selection);

    void DrawHierarchy(const glm::vec3& spawnPosition, SceneSelection& selection);
    void DrawInspector(SceneSelection& selection, bool& dirty);

    bool HasLightSelection(const SceneSelection& selection) const;
    Light* GetSelectedLight(const SceneSelection& selection);
    const Light* GetSelectedLight(const SceneSelection& selection) const;

private:
    Light MakeLight(const std::string& prefix, const glm::vec3& position, float radius, float helperSize, const glm::vec3& color) const;
    int NextSequence() const;

    std::vector<Light>* lights = nullptr;
    mutable int sequence = 0;
    bool duplicateLatch = false;
    bool deleteLatch = false;
};
