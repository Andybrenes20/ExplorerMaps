#pragma once

#include <functional>
#include <memory>
#include <string>
#include <cstdint>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>

#include "Camara.h"
#include "EditorTypes.h"
#include "GizmoSystem.h"
#include "LightEditor.h"
#include "PickingSystem.h"
#include "SceneSerializer.h"
#include "shaderClass.h"

struct EditorViewportRenderRequest
{
    int width = 1;
    int height = 1;
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);
    glm::vec3 cameraPosition = glm::vec3(0.0f);
};

struct EditorConfig
{
    GLFWwindow* window = nullptr;
    Camera* camera = nullptr;
    EditorSceneData* scene = nullptr;
    std::string sceneFilePath;
    std::string lightFilePath;
    std::function<void(const EditorViewportRenderRequest&)> viewportRenderer;
    float cameraFov = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 4000.0f;
};

class Editor
{
public:
    void Init(const EditorConfig& config);
    void Update(float deltaTime);
    void Render();
    void Shutdown();

    bool IsActive() const;
    bool WantsCursor() const;
    bool IsControllingViewportCamera() const;

private:
    struct ViewportState
    {
        ImVec2 origin = ImVec2(0.0f, 0.0f);
        ImVec2 size = ImVec2(1.0f, 1.0f);
        EditorViewportRenderRequest request;
        bool hovered = false;
    };

    void EnsureFramebuffer(int width, int height);
    void DestroyFramebuffer();
    void RenderViewportWindow();
    void RenderSceneHierarchyWindow();
    void RenderInspectorWindow();
    void RenderToolbarWindow();
    void RenderSceneToFramebuffer(const ImVec2& viewportSize);
    void RenderEditorHelpers(const EditorViewportRenderRequest& request);
    void TryPickViewport();
    bool MoveSelectedToViewportPoint(const ImVec2& mousePosition);
    bool ComputeViewportPlaneIntersection(const ImVec2& mousePosition, float planeY, glm::vec3& intersection) const;
    glm::vec3* GetSelectedPosition();
    void HandleSelectionKeyboardMove(float deltaTime);
    void SaveChanges();
    void RevertChanges();
    void AddHelper();
    void SyncHelperSequence();
    int ParseTrailingSequence(const std::string& value, const char* prefix) const;
    bool HasValidSelection() const;
    bool IsSelectionInsideBounds(const SceneSelection& selection) const;
    Camera* GetActiveEditorCamera() const;

    EditorConfig config;
    SceneSerializer serializer;
    PickingSystem pickingSystem;
    GizmoSystem gizmoSystem;
    LightEditor lightEditor;
    SceneSelection selection;
    ViewportState viewportState;

    bool active = false;
    bool toggleLatch = false;
    bool viewportCameraToggleLatch = false;
    bool viewportCameraControlActive = false;
    bool repositionSelectionArmed = false;
    bool dirty = false;
    int helperSequence = 0;

    GLuint framebuffer = 0;
    GLuint colorTexture = 0;
    GLuint depthStencil = 0;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    bool framebufferReady = false;

    std::unique_ptr<Camera> viewportCamera;
};
