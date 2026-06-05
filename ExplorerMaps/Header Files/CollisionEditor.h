#pragma once

#include <functional>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>

#include "Camara.h"
#include "ColliderManager.h"
#include "ColliderRenderer.h"
#include "ColliderSerializer.h"
#include "CollisionSystem.h"

struct CollisionViewportRenderRequest
{
    int width = 1;
    int height = 1;
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);
    glm::vec3 cameraPosition = glm::vec3(0.0f);
};

struct CollisionEditorConfig
{
    GLFWwindow* window = nullptr;
    Camera* camera = nullptr;
    ColliderManager* manager = nullptr;
    CollisionSystem* collisionSystem = nullptr;
    std::string filePath;
    std::function<void(const CollisionViewportRenderRequest&)> viewportRenderer;
    float cameraFov = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 4000.0f;
    float placementPlaneOffset = 6.0f;
    std::function<bool()> worldInteractionBlocked;
};

class CollisionEditor
{
public:
    void Init(const CollisionEditorConfig& initConfig);
    void Update(float deltaTime);
    void Render();
    void Shutdown();

    bool IsActive() const;
    bool WantsCursor() const;

private:
    enum class ColliderOperation
    {
        Move,
        Rotate,
        Scale
    };

    struct CreationState
    {
        bool armed = false;
        bool dragging = false;
        glm::vec3 startPoint = glm::vec3(0.0f);
        glm::vec3 rightAxis = glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 forwardAxis = glm::vec3(0.0f, 0.0f, 1.0f);
        BoxCollider preview;
    };

    struct ViewportState
    {
        ImVec2 origin = ImVec2(0.0f, 0.0f);
        ImVec2 size = ImVec2(1.0f, 1.0f);
        CollisionViewportRenderRequest request;
        bool hovered = false;
    };

    void EnsureFramebuffer(int width, int height);
    void DestroyFramebuffer();
    void RenderViewportWindow();
    void RenderHierarchyWindow();
    void HandleShortcuts();
    void HandleSelectionInput();
    void HandleCreationInput();
    void HandleSelectionKeyboardMove(float deltaTime);
    void ApplyGizmo();
    void RenderPanel();
    void RenderSceneToFramebuffer(const ImVec2& viewportSize);
    void AddCubeAtView();
    void SaveChanges();
    void LoadChanges();
    void DuplicateSelection();
    void DeleteSelection();
    void ClampSelectedScale();
    bool BuildWorldRay(const ImVec2& mousePosition, glm::vec3& rayOrigin, glm::vec3& rayDirection) const;
    bool SampleViewportWorldPoint(const ImVec2& mousePosition, glm::vec3& worldPoint) const;
    bool IntersectGroundPlane(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, float groundY, glm::vec3& hitPoint) const;
    BoxCollider* GetSelectedCollider();
    const BoxCollider* GetSelectedCollider() const;
    bool CanInteractWithWorld() const;
    void InvalidateSelectionIfNeeded();

    CollisionEditorConfig config;
    ColliderSerializer serializer;
    ColliderRenderer renderer;
    ColliderSelection selection;
    CreationState creationState;
    ViewportState viewportState;

    bool active = false;
    bool toggleLatch = false;
    bool dirty = false;
    bool autoSave = false;
    bool drawSolid = true;
    bool drawWireframe = true;
    bool drawThroughWalls = false;
    float collisionRadius = 6.0f;
    float newColliderHeight = 12.0f;
    ColliderOperation operation = ColliderOperation::Move;
    bool localTransformMode = true;
    bool useSnap = false;
    glm::vec3 moveSnap = glm::vec3(1.0f);
    float rotateSnap = 15.0f;
    float scaleSnap = 0.25f;
    bool gLatch = false;
    bool rLatch = false;
    bool sLatch = false;
    bool wLatch = false;
    bool eLatch = false;
    bool addLatch = false;
    bool deleteLatch = false;
    bool duplicateLatch = false;

    GLuint framebuffer = 0;
    GLuint colorTexture = 0;
    GLuint depthStencil = 0;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    bool framebufferReady = false;
};
