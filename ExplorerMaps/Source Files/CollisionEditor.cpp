#include "CollisionEditor.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "ImGuizmo.h"

namespace
{
    constexpr float kMaxRotationDeltaPerFrame = 10.0f;

    glm::mat4 ComposeGizmoMatrix(const BoxCollider& collider)
    {
        return ComposeColliderMatrix(collider);
    }

    float NormalizeAngleDelta(float degrees)
    {
        degrees = std::fmod(degrees + 180.0f, 360.0f);
        if (degrees < 0.0f)
        {
            degrees += 360.0f;
        }

        return degrees - 180.0f;
    }

    glm::vec3 ClampRotationDelta(const glm::vec3& delta)
    {
        glm::vec3 normalized(
            NormalizeAngleDelta(delta.x),
            NormalizeAngleDelta(delta.y),
            NormalizeAngleDelta(delta.z));

        const float maxComponent = std::max(
            std::abs(normalized.x),
            std::max(std::abs(normalized.y), std::abs(normalized.z)));
        if (maxComponent > kMaxRotationDeltaPerFrame)
        {
            normalized *= kMaxRotationDeltaPerFrame / maxComponent;
        }

        return normalized;
    }

    void ApplyGizmoMatrix(
        BoxCollider& collider,
        const glm::mat4& matrix,
        const glm::mat4& deltaMatrix,
        ImGuizmo::OPERATION operation)
    {
        float translation[3] = {};
        float rotation[3] = {};
        float scale[3] = {};
        ImGuizmo::DecomposeMatrixToComponents(&matrix[0][0], translation, rotation, scale);

        if (operation == ImGuizmo::TRANSLATE)
        {
            collider.position = glm::vec3(translation[0], translation[1], translation[2]);
        }
        else if (operation == ImGuizmo::ROTATE)
        {
            float deltaTranslation[3] = {};
            float deltaRotation[3] = {};
            float deltaScale[3] = {};
            ImGuizmo::DecomposeMatrixToComponents(&deltaMatrix[0][0], deltaTranslation, deltaRotation, deltaScale);
            collider.rotation += ClampRotationDelta(glm::vec3(deltaRotation[0], deltaRotation[1], deltaRotation[2]));
        }
        else if (operation == ImGuizmo::SCALE)
        {
            collider.scale = glm::max(glm::vec3(scale[0], scale[1], scale[2]), glm::vec3(0.1f));
        }
    }
}

void CollisionEditor::Init(const CollisionEditorConfig& initConfig)
{
    config = initConfig;
    serializer.Load(config.filePath, *config.manager);
    renderer.Init("Shaders/collider_debug.vert", "Shaders/collider_debug.frag");
}

void CollisionEditor::Update(float deltaTime)
{
    const bool togglePressed = glfwGetKey(config.window, GLFW_KEY_F8) == GLFW_PRESS;
    if (togglePressed && !toggleLatch)
    {
        active = !active;
        creationState.armed = false;
        creationState.dragging = false;
    }
    toggleLatch = togglePressed;

    if (!active)
    {
        return;
    }

    InvalidateSelectionIfNeeded();
    HandleShortcuts();
    HandleCreationInput();
    HandleSelectionInput();
    HandleSelectionKeyboardMove(deltaTime);
    ClampSelectedScale();

    if (autoSave && dirty)
    {
        SaveChanges();
    }
}

void CollisionEditor::Render()
{
    if (!active)
    {
        return;
    }

    RenderHierarchyWindow();
    RenderPanel();
    RenderViewportWindow();
}

void CollisionEditor::Shutdown()
{
    DestroyFramebuffer();
    renderer.Shutdown();
}

bool CollisionEditor::IsActive() const
{
    return active;
}

bool CollisionEditor::WantsCursor() const
{
    return active;
}

void CollisionEditor::EnsureFramebuffer(int width, int height)
{
    if (framebuffer != 0 && framebufferWidth == width && framebufferHeight == height)
    {
        return;
    }

    DestroyFramebuffer();

    framebufferWidth = width;
    framebufferHeight = height;
    framebufferReady = false;

    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, framebufferWidth, framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);

    glGenRenderbuffers(1, &depthStencil);
    glBindRenderbuffer(GL_RENDERBUFFER, depthStencil);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, framebufferWidth, framebufferHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencil);

    framebufferReady = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!framebufferReady)
    {
        DestroyFramebuffer();
    }
}

void CollisionEditor::DestroyFramebuffer()
{
    framebufferReady = false;

    if (depthStencil != 0)
    {
        glDeleteRenderbuffers(1, &depthStencil);
        depthStencil = 0;
    }

    if (colorTexture != 0)
    {
        glDeleteTextures(1, &colorTexture);
        colorTexture = 0;
    }

    if (framebuffer != 0)
    {
        glDeleteFramebuffers(1, &framebuffer);
        framebuffer = 0;
    }
}

void CollisionEditor::RenderViewportWindow()
{
    ImGui::SetNextWindowSize(ImVec2(900.0f, 620.0f), ImGuiCond_FirstUseEver);
    const ImGuiWindowFlags viewportFlags = ImGuiWindowFlags_NoMove;
    if (!ImGui::Begin("Collision Viewport", nullptr, viewportFlags))
    {
        ImGui::End();
        return;
    }

    ImVec2 available = ImGui::GetContentRegionAvail();
    available.x = std::max(available.x, 420.0f);
    available.y = std::max(available.y, 260.0f);

    if (ImGui::Button("Add Cube##viewport"))
    {
        AddCubeAtView();
    }
    ImGui::SameLine();
    int viewportOperationIndex = static_cast<int>(operation);
    ImGui::RadioButton("Move##viewport", &viewportOperationIndex, static_cast<int>(ColliderOperation::Move));
    ImGui::SameLine();
    ImGui::RadioButton("Rotate##viewport", &viewportOperationIndex, static_cast<int>(ColliderOperation::Rotate));
    ImGui::SameLine();
    ImGui::RadioButton("Scale##viewport", &viewportOperationIndex, static_cast<int>(ColliderOperation::Scale));
    operation = static_cast<ColliderOperation>(viewportOperationIndex);
    ImGui::SameLine();
    ImGui::Checkbox("Local##viewport", &localTransformMode);
    ImGui::SameLine();
    ImGui::Checkbox("Snap##viewport", &useSnap);

    available = ImGui::GetContentRegionAvail();
    available.x = std::max(available.x, 420.0f);
    available.y = std::max(available.y, 260.0f);

    RenderSceneToFramebuffer(available);
    if (!framebufferReady || colorTexture == 0)
    {
        ImGui::Dummy(available);
        ImGui::TextUnformatted("Viewport framebuffer unavailable.");
        ImGui::End();
        return;
    }

    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(colorTexture)), available, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
    viewportState.origin = ImGui::GetItemRectMin();
    viewportState.size = ImGui::GetItemRectSize();
    viewportState.hovered = ImGui::IsItemHovered();
    ApplyGizmo();

    if (creationState.armed || creationState.dragging)
    {
        ImGui::TextUnformatted("Create mode: click and drag in this viewport. Right click cancels.");
    }
    else
    {
        ImGui::TextUnformatted("Click a collider to select. White lines show existing collisions.");
    }

    ImGui::End();
}

void CollisionEditor::RenderHierarchyWindow()
{
    ImGui::SetNextWindowSize(ImVec2(320.0f, 520.0f), ImGuiCond_FirstUseEver);
    const ImGuiWindowFlags hierarchyFlags = ImGuiWindowFlags_NoMove;
    if (!ImGui::Begin("Collision Hierarchy", nullptr, hierarchyFlags))
    {
        ImGui::End();
        return;
    }

    for (int i = 0; i < static_cast<int>(config.manager->GetColliders().size()); ++i)
    {
        const BoxCollider& collider = config.manager->GetColliders()[i];
        if (ImGui::Selectable(collider.name.c_str(), selection.index == i))
        {
            selection.index = i;
        }
    }

    ImGui::End();
}

void CollisionEditor::HandleShortcuts()
{
    const ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard && !ImGui::IsAnyItemActive())
    {
        const bool gPressed = glfwGetKey(config.window, GLFW_KEY_G) == GLFW_PRESS;
        const bool rPressed = glfwGetKey(config.window, GLFW_KEY_R) == GLFW_PRESS;
        const bool sPressed = glfwGetKey(config.window, GLFW_KEY_S) == GLFW_PRESS;
        const bool wPressed = glfwGetKey(config.window, GLFW_KEY_W) == GLFW_PRESS;
        const bool ePressed = glfwGetKey(config.window, GLFW_KEY_E) == GLFW_PRESS;
        const bool addPressed = glfwGetKey(config.window, GLFW_KEY_B) == GLFW_PRESS;
        const bool deletePressed = glfwGetKey(config.window, GLFW_KEY_DELETE) == GLFW_PRESS;
        const bool ctrlDown = glfwGetKey(config.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
            glfwGetKey(config.window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
        const bool duplicatePressed = ctrlDown && glfwGetKey(config.window, GLFW_KEY_D) == GLFW_PRESS;

        if (gPressed && !gLatch)
        {
            operation = ColliderOperation::Move;
        }
        if (wPressed && !wLatch)
        {
            operation = ColliderOperation::Move;
        }
        if (ePressed && !eLatch)
        {
            operation = ColliderOperation::Rotate;
        }
        if (rPressed && !rLatch)
        {
            operation = ColliderOperation::Rotate;
        }
        if (sPressed && !sLatch)
        {
            operation = ColliderOperation::Scale;
        }
        if (addPressed && !addLatch)
        {
            AddCubeAtView();
        }
        if (deletePressed && !deleteLatch)
        {
            DeleteSelection();
        }
        if (duplicatePressed && !duplicateLatch)
        {
            DuplicateSelection();
        }

        gLatch = gPressed;
        rLatch = rPressed;
        sLatch = sPressed;
        wLatch = wPressed;
        eLatch = ePressed;
        addLatch = addPressed;
        deleteLatch = deletePressed;
        duplicateLatch = duplicatePressed;
    }
    else
    {
        gLatch = false;
        rLatch = false;
        sLatch = false;
        wLatch = false;
        eLatch = false;
        addLatch = false;
        deleteLatch = false;
        duplicateLatch = false;
    }

    if (viewportState.hovered && ImGui::IsMouseClicked(1))
    {
        creationState.armed = false;
        creationState.dragging = false;
    }
}

void CollisionEditor::HandleSelectionInput()
{
    if (!CanInteractWithWorld() || creationState.armed || creationState.dragging || !viewportState.hovered)
    {
        return;
    }

    if (ImGuizmo::IsOver() || !ImGui::IsMouseClicked(0))
    {
        return;
    }

    glm::vec3 rayOrigin(0.0f);
    glm::vec3 rayDirection(0.0f);
    if (!BuildWorldRay(ImGui::GetMousePos(), rayOrigin, rayDirection))
    {
        return;
    }

    int colliderIndex = -1;
    float distance = 0.0f;
    if (config.collisionSystem->Raycast(config.manager->GetColliders(), rayOrigin, rayDirection, colliderIndex, distance))
    {
        selection.index = colliderIndex;
    }
    else
    {
        selection.index = -1;
    }
}

void CollisionEditor::HandleCreationInput()
{
    if ((!creationState.armed && !creationState.dragging) || !CanInteractWithWorld() || !viewportState.hovered)
    {
        return;
    }

    glm::vec3 rayOrigin(0.0f);
    glm::vec3 rayDirection(0.0f);
    if (!BuildWorldRay(ImGui::GetMousePos(), rayOrigin, rayDirection))
    {
        return;
    }

    if (creationState.armed && ImGui::IsMouseClicked(0))
    {
        glm::vec3 hitPoint(0.0f);
        if (!SampleViewportWorldPoint(ImGui::GetMousePos(), hitPoint))
        {
            const float fallbackPlaneY = config.camera->Position.y - config.placementPlaneOffset;
            if (!IntersectGroundPlane(rayOrigin, rayDirection, fallbackPlaneY, hitPoint))
            {
                return;
            }
        }

        glm::vec3 forwardAxis = glm::vec3(config.camera->Orientation.x, 0.0f, config.camera->Orientation.z);
        if (glm::length(forwardAxis) < 0.0001f)
        {
            forwardAxis = glm::vec3(0.0f, 0.0f, -1.0f);
        }
        forwardAxis = glm::normalize(forwardAxis);
        const glm::vec3 upAxis(0.0f, 1.0f, 0.0f);
        glm::vec3 rightAxis = glm::normalize(glm::cross(forwardAxis, upAxis));
        if (glm::length(rightAxis) < 0.0001f)
        {
            rightAxis = glm::vec3(1.0f, 0.0f, 0.0f);
        }

        creationState.dragging = true;
        creationState.startPoint = hitPoint;
        creationState.rightAxis = rightAxis;
        creationState.forwardAxis = forwardAxis;
        creationState.preview.position = glm::vec3(hitPoint.x, hitPoint.y + newColliderHeight * 0.5f, hitPoint.z);
        creationState.preview.scale = glm::vec3(1.0f, newColliderHeight, 1.0f);
        creationState.preview.rotation = glm::vec3(
            0.0f,
            glm::degrees(std::atan2(forwardAxis.x, forwardAxis.z)),
            0.0f);
        creationState.preview.name = "Preview";
        creationState.preview.id.clear();
    }

    if (!creationState.dragging)
    {
        return;
    }

    glm::vec3 hitPoint(0.0f);
    if (!IntersectGroundPlane(rayOrigin, rayDirection, creationState.startPoint.y, hitPoint))
    {
        return;
    }

    const glm::vec3 delta = hitPoint - creationState.startPoint;
    const float localRight = glm::dot(delta, creationState.rightAxis);
    const float localForward = glm::dot(delta, creationState.forwardAxis);
    const glm::vec3 footprintCenter =
        creationState.startPoint +
        creationState.rightAxis * (localRight * 0.5f) +
        creationState.forwardAxis * (localForward * 0.5f);

    creationState.preview.position = glm::vec3(
        footprintCenter.x,
        creationState.startPoint.y + newColliderHeight * 0.5f,
        footprintCenter.z);
    creationState.preview.scale = glm::vec3(
        std::max(std::abs(localRight), 1.0f),
        newColliderHeight,
        std::max(std::abs(localForward), 1.0f));

    if (ImGui::IsMouseReleased(0))
    {
        selection.index = config.manager->AddBox(
            creationState.preview.position,
            creationState.preview.scale,
            creationState.preview.rotation);
        creationState.armed = false;
        creationState.dragging = false;
        dirty = true;
    }
}

void CollisionEditor::HandleSelectionKeyboardMove(float deltaTime)
{
    if (!selection.IsValid() || creationState.dragging)
    {
        return;
    }

    BoxCollider* selectedCollider = GetSelectedCollider();
    if (!selectedCollider)
    {
        return;
    }

    if (ImGui::IsAnyItemActive())
    {
        return;
    }

    float moveStep = 12.0f * deltaTime;
    const bool shiftDown = glfwGetKey(config.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(config.window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    const bool ctrlDown = glfwGetKey(config.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
        glfwGetKey(config.window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

    if (shiftDown)
    {
        moveStep *= 2.0f;
    }
    if (ctrlDown)
    {
        moveStep *= 0.2f;
    }

    glm::vec3 movement(0.0f);
    if (glfwGetKey(config.window, GLFW_KEY_LEFT) == GLFW_PRESS)
    {
        movement.x -= moveStep;
    }
    if (glfwGetKey(config.window, GLFW_KEY_RIGHT) == GLFW_PRESS)
    {
        movement.x += moveStep;
    }
    if (glfwGetKey(config.window, GLFW_KEY_UP) == GLFW_PRESS)
    {
        movement.z -= moveStep;
    }
    if (glfwGetKey(config.window, GLFW_KEY_DOWN) == GLFW_PRESS)
    {
        movement.z += moveStep;
    }
    if (glfwGetKey(config.window, GLFW_KEY_PAGE_UP) == GLFW_PRESS || glfwGetKey(config.window, GLFW_KEY_U) == GLFW_PRESS)
    {
        movement.y += moveStep;
    }
    if (glfwGetKey(config.window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS || glfwGetKey(config.window, GLFW_KEY_O) == GLFW_PRESS)
    {
        movement.y -= moveStep;
    }

    if (movement.x == 0.0f && movement.y == 0.0f && movement.z == 0.0f)
    {
        return;
    }

    selectedCollider->position += movement;
    dirty = true;
}

void CollisionEditor::ApplyGizmo()
{
    if (!CanInteractWithWorld() || creationState.dragging || !selection.IsValid())
    {
        return;
    }

    BoxCollider* selectedCollider = GetSelectedCollider();
    if (!selectedCollider)
    {
        return;
    }

    ImGuizmo::BeginFrame();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(viewportState.origin.x, viewportState.origin.y, viewportState.size.x, viewportState.size.y);

    glm::mat4 matrix = ComposeGizmoMatrix(*selectedCollider);
    ImGuizmo::OPERATION gizmoOperation = ImGuizmo::TRANSLATE;
    if (operation == ColliderOperation::Rotate)
    {
        gizmoOperation = ImGuizmo::ROTATE;
    }
    else if (operation == ColliderOperation::Scale)
    {
        gizmoOperation = ImGuizmo::SCALE;
    }

    ImGuizmo::MODE gizmoMode = localTransformMode ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
    if (gizmoOperation == ImGuizmo::SCALE)
    {
        gizmoMode = ImGuizmo::LOCAL;
    }

    float snapValues[3] =
    {
        gizmoOperation == ImGuizmo::ROTATE ? rotateSnap : moveSnap.x,
        gizmoOperation == ImGuizmo::ROTATE ? rotateSnap : moveSnap.y,
        gizmoOperation == ImGuizmo::ROTATE ? rotateSnap : moveSnap.z
    };
    if (gizmoOperation == ImGuizmo::SCALE)
    {
        snapValues[0] = scaleSnap;
        snapValues[1] = scaleSnap;
        snapValues[2] = scaleSnap;
    }

    glm::mat4 deltaMatrix(1.0f);
    ImGuizmo::Manipulate(
        &viewportState.request.view[0][0],
        &viewportState.request.projection[0][0],
        gizmoOperation,
        gizmoMode,
        &matrix[0][0],
        &deltaMatrix[0][0],
        useSnap ? snapValues : nullptr);

    if (ImGuizmo::IsUsing())
    {
        ApplyGizmoMatrix(*selectedCollider, matrix, deltaMatrix, gizmoOperation);
        dirty = true;
    }
}

void CollisionEditor::RenderPanel()
{
    ImGui::SetNextWindowSize(ImVec2(430.0f, 620.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Collision Inspector"))
    {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("F8 toggles collision editor");
    ImGui::Text("State: %s", dirty ? "Unsaved changes" : "Saved");
    ImGui::Text("Colliders: %d", static_cast<int>(config.manager->GetColliders().size()));
    ImGui::Checkbox("Auto save", &autoSave);
    ImGui::Checkbox("Solid fill", &drawSolid);
    ImGui::Checkbox("White wireframe", &drawWireframe);
    ImGui::Checkbox("X-Ray", &drawThroughWalls);
    ImGui::DragFloat("Camera collision radius", &collisionRadius, 0.1f, 0.5f, 32.0f, "%.2f");
    ImGui::DragFloat("New collider height", &newColliderHeight, 0.25f, 1.0f, 200.0f, "%.2f");

    if (ImGui::Button("Add Cube"))
    {
        AddCubeAtView();
    }
    ImGui::SameLine();
    if (ImGui::Button(creationState.armed || creationState.dragging ? "Cancel Box" : "Create Box"))
    {
        if (creationState.armed || creationState.dragging)
        {
            creationState.armed = false;
            creationState.dragging = false;
        }
        else
        {
            creationState.armed = true;
            creationState.dragging = false;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save"))
    {
        SaveChanges();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load"))
    {
        LoadChanges();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Transform mode");
    int operationIndex = static_cast<int>(operation);
    ImGui::RadioButton("Move##collision", &operationIndex, static_cast<int>(ColliderOperation::Move));
    ImGui::SameLine();
    ImGui::RadioButton("Rotate##collision", &operationIndex, static_cast<int>(ColliderOperation::Rotate));
    ImGui::SameLine();
    ImGui::RadioButton("Scale##collision", &operationIndex, static_cast<int>(ColliderOperation::Scale));
    operation = static_cast<ColliderOperation>(operationIndex);
    ImGui::Checkbox("Local axes", &localTransformMode);
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &useSnap);
    if (operation == ColliderOperation::Move)
    {
        ImGui::DragFloat3("Move snap", &moveSnap.x, 0.1f, 0.1f, 100.0f, "%.2f");
    }
    else if (operation == ColliderOperation::Rotate)
    {
        ImGui::DragFloat("Rotate snap", &rotateSnap, 0.5f, 1.0f, 180.0f, "%.1f");
    }
    else
    {
        ImGui::DragFloat("Scale snap", &scaleSnap, 0.01f, 0.01f, 10.0f, "%.2f");
    }

    BoxCollider* selectedCollider = GetSelectedCollider();
    if (selectedCollider)
    {
        ImGui::Separator();
        ImGui::Text("Selected: %s", selectedCollider->name.c_str());
        char nameBuffer[128] = {};
        strncpy_s(nameBuffer, selectedCollider->name.c_str(), _TRUNCATE);
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
        {
            selectedCollider->name = nameBuffer;
            dirty = true;
        }

        dirty |= ImGui::DragFloat3("Position", &selectedCollider->position.x, 0.25f);
        dirty |= ImGui::DragFloat3("Rotation", &selectedCollider->rotation.x, 0.5f);
        dirty |= ImGui::DragFloat3("Scale", &selectedCollider->scale.x, 0.05f, 0.1f, 1000.0f);
        dirty |= ImGui::Checkbox("Blocking", &selectedCollider->blocking);
        dirty |= ImGui::Checkbox("Visible", &selectedCollider->visible);

        if (ImGui::Button("Duplicate"))
        {
            DuplicateSelection();
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete"))
        {
            DeleteSelection();
        }
    }
    else
    {
        ImGui::Separator();
        ImGui::TextUnformatted("Select a collider in the viewport or hierarchy.");
    }

    ImGui::Separator();
    ImGui::BulletText("Create Box + drag in Collision Viewport");
    ImGui::BulletText("White lines = existing colliders");
    ImGui::BulletText("Green = selected, red = overlap");
    ImGui::BulletText("Delete removes selected collider");
    ImGui::BulletText("Ctrl+D duplicates selected collider");
    ImGui::BulletText("B adds a cube in front of the camera");
    ImGui::BulletText("G/W move, R/E rotate, S scale");
    ImGui::BulletText("Arrows move selected collider. PgUp/PgDn or U/O move height");

    ImGui::End();
}

void CollisionEditor::RenderSceneToFramebuffer(const ImVec2& viewportSize)
{
    const int targetWidth = std::max(static_cast<int>(viewportSize.x), 1);
    const int targetHeight = std::max(static_cast<int>(viewportSize.y), 1);
    EnsureFramebuffer(targetWidth, targetHeight);
    if (!framebufferReady)
    {
        return;
    }

    viewportState.request.width = targetWidth;
    viewportState.request.height = targetHeight;
    viewportState.request.cameraPosition = config.camera->Position;
    viewportState.request.view = config.camera->GetViewMatrix();
    viewportState.request.projection = glm::perspective(
        glm::radians(config.cameraFov),
        static_cast<float>(targetWidth) / static_cast<float>(targetHeight),
        config.nearPlane,
        config.farPlane);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, targetWidth, targetHeight);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.09f, 0.11f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (config.viewportRenderer)
    {
        config.viewportRenderer(viewportState.request);
    }

    const BoxCollider* previewCollider = creationState.dragging ? &creationState.preview : nullptr;
    renderer.Render(
        config.manager->GetColliders(),
        previewCollider,
        *config.camera,
        config.cameraFov,
        config.nearPlane,
        config.farPlane,
        *config.collisionSystem,
        selection.index,
        config.camera->Position,
        collisionRadius,
        drawSolid,
        drawWireframe,
        drawThroughWalls);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    int windowWidth = 0;
    int windowHeight = 0;
    glfwGetFramebufferSize(config.window, &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);
}

void CollisionEditor::AddCubeAtView()
{
    if (!config.manager || !config.camera)
    {
        return;
    }

    glm::vec3 forward = config.camera->Orientation;
    if (glm::length(forward) < 0.0001f)
    {
        forward = glm::vec3(0.0f, 0.0f, -1.0f);
    }
    forward = glm::normalize(forward);

    glm::vec3 horizontalForward(forward.x, 0.0f, forward.z);
    if (glm::length(horizontalForward) < 0.0001f)
    {
        horizontalForward = glm::vec3(0.0f, 0.0f, -1.0f);
    }
    horizontalForward = glm::normalize(horizontalForward);

    const glm::vec3 scale(6.0f, std::max(newColliderHeight, 1.0f), 6.0f);
    glm::vec3 position = config.camera->Position + horizontalForward * 24.0f;
    position.y = config.camera->Position.y - config.placementPlaneOffset + scale.y * 0.5f;

    selection.index = config.manager->AddBox(position, scale, glm::vec3(0.0f));
    operation = ColliderOperation::Move;
    creationState.armed = false;
    creationState.dragging = false;
    dirty = true;
}

void CollisionEditor::SaveChanges()
{
    if (serializer.Save(config.filePath, *config.manager))
    {
        dirty = false;
    }
}

void CollisionEditor::LoadChanges()
{
    serializer.Load(config.filePath, *config.manager);
    selection.index = -1;
    dirty = false;
    creationState.armed = false;
    creationState.dragging = false;
}

void CollisionEditor::DuplicateSelection()
{
    if (!selection.IsValid())
    {
        return;
    }

    const int duplicateIndex = config.manager->Duplicate(selection.index);
    if (duplicateIndex >= 0)
    {
        selection.index = duplicateIndex;
        dirty = true;
    }
}

void CollisionEditor::DeleteSelection()
{
    if (!selection.IsValid())
    {
        return;
    }

    if (config.manager->Remove(selection.index))
    {
        selection.index = std::min(selection.index, static_cast<int>(config.manager->GetColliders().size()) - 1);
        dirty = true;
    }
}

void CollisionEditor::ClampSelectedScale()
{
    BoxCollider* selectedCollider = GetSelectedCollider();
    if (!selectedCollider)
    {
        return;
    }

    selectedCollider->scale = glm::max(selectedCollider->scale, glm::vec3(0.1f));
}

bool CollisionEditor::BuildWorldRay(const ImVec2& mousePosition, glm::vec3& rayOrigin, glm::vec3& rayDirection) const
{
    const float localX = mousePosition.x - viewportState.origin.x;
    const float localY = mousePosition.y - viewportState.origin.y;
    if (localX < 0.0f || localY < 0.0f || localX > viewportState.size.x || localY > viewportState.size.y)
    {
        return false;
    }

    const float ndcX = (2.0f * localX / viewportState.size.x) - 1.0f;
    const float ndcY = 1.0f - (2.0f * localY / viewportState.size.y);

    const glm::mat4 inverseViewProjection = glm::inverse(viewportState.request.projection * viewportState.request.view);
    glm::vec4 nearPoint = inverseViewProjection * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farPoint = inverseViewProjection * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;

    rayOrigin = viewportState.request.cameraPosition;
    rayDirection = glm::normalize(glm::vec3(farPoint - nearPoint));
    return true;
}

bool CollisionEditor::SampleViewportWorldPoint(const ImVec2& mousePosition, glm::vec3& worldPoint) const
{
    if (!framebufferReady || framebuffer == 0)
    {
        return false;
    }

    const float localX = mousePosition.x - viewportState.origin.x;
    const float localY = mousePosition.y - viewportState.origin.y;
    if (localX < 0.0f || localY < 0.0f || localX > viewportState.size.x || localY > viewportState.size.y)
    {
        return false;
    }

    const int pixelX = std::clamp(static_cast<int>(localX), 0, std::max(framebufferWidth - 1, 0));
    const int pixelY = std::clamp(static_cast<int>(viewportState.size.y - localY), 0, std::max(framebufferHeight - 1, 0));

    float depth = 1.0f;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glReadPixels(pixelX, pixelY, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (depth >= 0.9999f)
    {
        return false;
    }

    const float ndcX = (2.0f * localX / viewportState.size.x) - 1.0f;
    const float ndcY = 1.0f - (2.0f * localY / viewportState.size.y);
    const float ndcZ = depth * 2.0f - 1.0f;

    const glm::mat4 inverseViewProjection = glm::inverse(viewportState.request.projection * viewportState.request.view);
    glm::vec4 world = inverseViewProjection * glm::vec4(ndcX, ndcY, ndcZ, 1.0f);
    if (std::abs(world.w) < 0.0001f)
    {
        return false;
    }

    world /= world.w;
    worldPoint = glm::vec3(world);
    return true;
}

bool CollisionEditor::IntersectGroundPlane(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, float groundY, glm::vec3& hitPoint) const
{
    if (std::abs(rayDirection.y) < 0.0001f)
    {
        return false;
    }

    const float t = (groundY - rayOrigin.y) / rayDirection.y;
    if (t <= 0.0f)
    {
        return false;
    }

    hitPoint = rayOrigin + rayDirection * t;
    return true;
}

BoxCollider* CollisionEditor::GetSelectedCollider()
{
    return config.manager->Get(selection.index);
}

const BoxCollider* CollisionEditor::GetSelectedCollider() const
{
    return config.manager->Get(selection.index);
}

bool CollisionEditor::CanInteractWithWorld() const
{
    return !config.worldInteractionBlocked || !config.worldInteractionBlocked();
}

void CollisionEditor::InvalidateSelectionIfNeeded()
{
    if (!config.manager->IsValidIndex(selection.index))
    {
        selection.index = -1;
    }
}
