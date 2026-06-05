#pragma once

#include <string>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <imgui.h>

#include "EditorTypes.h"
#include "LightGizmoRenderer.h"
#include "LightManager.h"
#include "LightSerializer.h"

struct EditorViewportRenderRequest;

struct LightEditorConfig
{
    GLFWwindow* window = nullptr;
    std::vector<Light>* lights = nullptr;
    std::string lightFilePath;
};

class LightEditor
{
public:
    void Init(const LightEditorConfig& config);
    void Shutdown();

    void Update(const glm::vec3& spawnPosition, SceneSelection& selection, bool& dirty);
    void RenderHierarchy(const glm::vec3& spawnPosition, SceneSelection& selection, bool& dirty);
    void RenderInspector(SceneSelection& selection, bool& dirty);
    void RenderToolbar(SceneSelection& selection, const glm::vec3& spawnPosition, bool& dirty);
    void RenderViewportHelpers(const EditorViewportRenderRequest& request) const;
    bool HandleViewportClick(const ImVec2& mousePosition, const ImVec2& viewportOrigin, const ImVec2& viewportSize, const EditorViewportRenderRequest& request, SceneSelection& selection, bool& dirty);
    bool IsPlacementActive() const;
    bool ConsumedNewSelection();
    void CancelPlacement();

    bool Save();
    bool SaveToFile(const std::string& filePath) const;
    void CommitSavedState();
    void Revert(std::vector<Light>& lights, SceneSelection& selection);
    void SyncSelection(SceneSelection& selection);

private:
    enum class PlacementMode
    {
        None,
        PointLight,
        AreaLight,
        CubeLight,
        MoveSelected
    };

    glm::vec3 ComputePlacementPosition(const ImVec2& mousePosition, const ImVec2& viewportOrigin, const ImVec2& viewportSize, const EditorViewportRenderRequest& request) const;
    int CreateRequestedLight(const glm::vec3& position);

    LightEditorConfig config;
    LightManager manager;
    LightSerializer serializer;
    LightGizmoRenderer renderer;
    std::vector<Light> savedLights;
    PlacementMode placementMode = PlacementMode::None;
    bool shortcutCreateLatch = false;
    bool newSelectionCreated = false;
};
