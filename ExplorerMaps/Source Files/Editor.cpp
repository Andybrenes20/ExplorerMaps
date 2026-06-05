#include "Editor.h"

#include <filesystem>
#include <system_error>

#include "ImGuizmo.h"

namespace
{
    namespace fs = std::filesystem;

    std::string MakeTempFilePath(const std::string& filePath)
    {
        return filePath + ".tmp";
    }

    std::string MakeBackupFilePath(const std::string& filePath)
    {
        return filePath + ".bak";
    }

    bool CommitStagedFiles(const std::string& sceneTempPath, const std::string& scenePath, const std::string& lightTempPath, const std::string& lightPath)
    {
        std::error_code ec;

        const std::string targets[2] = { scenePath, lightPath };
        const std::string staged[2] = { sceneTempPath, lightTempPath };
        const std::string backups[2] = { MakeBackupFilePath(scenePath), MakeBackupFilePath(lightPath) };
        bool hadOriginal[2] = { false, false };
        bool committed[2] = { false, false };

        for (int i = 0; i < 2; ++i)
        {
            hadOriginal[i] = fs::exists(targets[i], ec) && !ec;
            ec.clear();
            if (!hadOriginal[i])
            {
                continue;
            }

            fs::remove(backups[i], ec);
            ec.clear();
            fs::rename(targets[i], backups[i], ec);
            if (ec)
            {
                return false;
            }
        }

        for (int i = 0; i < 2; ++i)
        {
            fs::remove(targets[i], ec);
            ec.clear();
            fs::rename(staged[i], targets[i], ec);
            if (ec)
            {
                for (int rollback = 0; rollback <= i; ++rollback)
                {
                    if (committed[rollback])
                    {
                        fs::remove(targets[rollback], ec);
                        ec.clear();
                    }

                    if (hadOriginal[rollback])
                    {
                        fs::rename(backups[rollback], targets[rollback], ec);
                        ec.clear();
                    }
                }

                return false;
            }

            committed[i] = true;
        }

        for (int i = 0; i < 2; ++i)
        {
            if (hadOriginal[i])
            {
                fs::remove(backups[i], ec);
                ec.clear();
            }
        }

        return true;
    }
}

void Editor::Init(const EditorConfig& initConfig)
{
    config = initConfig;
    serializer.Load(config.sceneFilePath, *config.scene);
    SyncHelperSequence();
    viewportCamera = std::make_unique<Camera>(*config.camera);
    viewportCamera->firstClick = true;

    lightEditor.Init({ config.window, &config.scene->lights, config.lightFilePath });
    lightEditor.SyncSelection(selection);
}

void Editor::Update(float deltaTime)
{
    const bool togglePressed = glfwGetKey(config.window, GLFW_KEY_F1) == GLFW_PRESS;
    if (togglePressed && !toggleLatch)
    {
        active = !active;
        if (!active)
        {
            viewportCameraControlActive = false;
            if (viewportCamera)
            {
                viewportCamera->firstClick = true;
            }
        }
    }

    toggleLatch = togglePressed;
    if (!active)
    {
        return;
    }

    const bool viewportTogglePressed = glfwGetKey(config.window, GLFW_KEY_F6) == GLFW_PRESS;
    if (viewportTogglePressed && !viewportCameraToggleLatch)
    {
        viewportCameraControlActive = !viewportCameraControlActive;
        if (viewportCameraControlActive && viewportCamera)
        {
            *viewportCamera = *config.camera;
        }

        if (viewportCamera)
        {
            viewportCamera->firstClick = true;
        }
    }
    viewportCameraToggleLatch = viewportTogglePressed;

    if (viewportCameraControlActive && viewportCamera)
    {
        viewportCamera->width = std::max(framebufferWidth, config.camera->width);
        viewportCamera->height = std::max(framebufferHeight, config.camera->height);
        viewportCamera->Inputs(config.window, deltaTime);
        return;
    }

    gizmoSystem.HandleShortcuts();
    Camera* activeEditorCamera = GetActiveEditorCamera();
    lightEditor.Update(activeEditorCamera->Position + activeEditorCamera->Orientation * 35.0f, selection, dirty);
    HandleSelectionKeyboardMove(deltaTime);
    if (lightEditor.ConsumedNewSelection())
    {
        gizmoSystem.SetTranslateMode();
    }
}

void Editor::Render()
{
    if (!active)
    {
        return;
    }

    RenderToolbarWindow();
    RenderSceneHierarchyWindow();
    RenderInspectorWindow();
    RenderViewportWindow();
}

void Editor::Shutdown()
{
    DestroyFramebuffer();
    lightEditor.Shutdown();
}

bool Editor::IsActive() const
{
    return active;
}

bool Editor::WantsCursor() const
{
    return active && !viewportCameraControlActive;
}

bool Editor::IsControllingViewportCamera() const
{
    return active && viewportCameraControlActive;
}

void Editor::EnsureFramebuffer(int width, int height)
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

void Editor::DestroyFramebuffer()
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

void Editor::RenderViewportWindow()
{
    ImGui::SetNextWindowSize(ImVec2(760.0f, 480.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Editor Viewport"))
    {
        ImGui::End();
        return;
    }

    ImVec2 available = ImGui::GetContentRegionAvail();
    available.x = std::max(available.x, 320.0f);
    available.y = std::max(available.y, 220.0f);

    RenderSceneToFramebuffer(available);

    if (!framebufferReady || colorTexture == 0)
    {
        ImGui::Dummy(available);
        viewportState.origin = ImGui::GetItemRectMin();
        viewportState.size = ImGui::GetItemRectSize();
        viewportState.hovered = ImGui::IsItemHovered();
        ImGui::SetCursorScreenPos(ImVec2(viewportState.origin.x + 12.0f, viewportState.origin.y + 12.0f));
        ImGui::TextUnformatted("Viewport framebuffer is unavailable.");
        ImGui::End();
        return;
    }

    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(colorTexture)), available, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
    viewportState.origin = ImGui::GetItemRectMin();
    viewportState.size = ImGui::GetItemRectSize();
    viewportState.hovered = ImGui::IsItemHovered();

    if (viewportState.hovered && ImGui::IsMouseClicked(0) && !gizmoSystem.IsUsing() && !gizmoSystem.IsOver())
    {
        if (repositionSelectionArmed)
        {
            if (MoveSelectedToViewportPoint(ImGui::GetMousePos()))
            {
                repositionSelectionArmed = false;
            }
        }
        else if (lightEditor.HandleViewportClick(ImGui::GetMousePos(), viewportState.origin, viewportState.size, viewportState.request, selection, dirty))
        {
            gizmoSystem.SetTranslateMode();
            lightEditor.SyncSelection(selection);
        }
        else
        {
            TryPickViewport();
        }
    }
    else if (viewportState.hovered && ImGui::IsMouseClicked(1) && (lightEditor.IsPlacementActive() || repositionSelectionArmed))
    {
        lightEditor.CancelPlacement();
        repositionSelectionArmed = false;
        lightEditor.SyncSelection(selection);
    }

    const bool changedByGizmo = gizmoSystem.Draw(
        *config.scene,
        selection,
        GizmoViewport{ viewportState.origin, viewportState.size, viewportState.request.view, viewportState.request.projection });
    dirty = dirty || changedByGizmo;
    lightEditor.SyncSelection(selection);

    ImGui::End();
}

void Editor::RenderSceneHierarchyWindow()
{
    ImGui::SetNextWindowSize(ImVec2(320.0f, 460.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Editor Hierarchy"))
    {
        ImGui::End();
        return;
    }

    if (ImGui::CollapsingHeader("Entities", ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (int i = 0; i < static_cast<int>(config.scene->entities.size()); ++i)
        {
            const bool selected = selection.type == SceneObjectType::Entity && selection.index == i;
            if (ImGui::Selectable(config.scene->entities[i].name.c_str(), selected))
            {
                selection = { SceneObjectType::Entity, i };
            }
        }
    }

    if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_DefaultOpen))
    {
        Camera* activeEditorCamera = GetActiveEditorCamera();
        lightEditor.RenderHierarchy(activeEditorCamera->Position + activeEditorCamera->Orientation * 35.0f, selection, dirty);
    }

    if (ImGui::CollapsingHeader("Helpers", ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (int i = 0; i < static_cast<int>(config.scene->helpers.size()); ++i)
        {
            const bool selected = selection.type == SceneObjectType::Helper && selection.index == i;
            if (ImGui::Selectable(config.scene->helpers[i].name.c_str(), selected))
            {
                selection = { SceneObjectType::Helper, i };
            }
        }
    }

    lightEditor.SyncSelection(selection);
    ImGui::End();
}

void Editor::RenderInspectorWindow()
{
    ImGui::SetNextWindowSize(ImVec2(360.0f, 480.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Inspector"))
    {
        ImGui::End();
        return;
    }

    if (!HasValidSelection())
    {
        ImGui::TextUnformatted("Select an entity, light or helper.");
        ImGui::End();
        return;
    }

    if (selection.type == SceneObjectType::Entity)
    {
        Entity& entity = config.scene->entities[selection.index];
        dirty |= ImGui::DragFloat3("Position", &entity.position.x, 0.25f);
        dirty |= ImGui::DragFloat3("Rotation", &entity.rotation.x, 0.5f);
        dirty |= ImGui::DragFloat3("Scale", &entity.scale.x, 0.01f, 0.01f, 100.0f);
    }
    else if (selection.type == SceneObjectType::Light)
    {
        lightEditor.RenderInspector(selection, dirty);
    }
    else if (selection.type == SceneObjectType::Helper)
    {
        Helper& helper = config.scene->helpers[selection.index];
        dirty |= ImGui::DragFloat3("Position", &helper.position.x, 0.25f);
        dirty |= ImGui::DragFloat3("Rotation", &helper.rotation.x, 0.5f);
        dirty |= ImGui::DragFloat3("Scale", &helper.scale.x, 0.05f, 0.1f, 300.0f);
        dirty |= ImGui::ColorEdit3("Color", &helper.color.x);
    }

    if (selection.type == SceneObjectType::Light &&
        gizmoSystem.GetCurrentOperation() == ImGuizmo::ROTATE)
    {
        ImGui::Separator();
        ImGui::TextUnformatted("Light rotation is disabled. Rotate switches to translate for lights.");
    }

    ImGui::End();
}

void Editor::RenderToolbarWindow()
{
    ImGui::SetNextWindowSize(ImVec2(700.0f, 180.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Editor"))
    {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("F1 toggles the editor. Camera/player controls stay paused while active.");
    ImGui::Text("F6: %s viewport camera", viewportCameraControlActive ? "exit" : "control");
    ImGui::Text("State: %s", dirty ? "Unsaved changes" : "Saved");

    if (ImGui::Button("Save scene"))
    {
        SaveChanges();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel changes"))
    {
        RevertChanges();
    }

    ImGui::SameLine();
    if (ImGui::Button("Add helper"))
    {
        AddHelper();
    }

    ImGui::SameLine();
    const bool canMoveSelection = HasValidSelection();
    if (canMoveSelection && ImGui::Button(repositionSelectionArmed ? "Click In Viewport..." : "Move Selected"))
    {
        repositionSelectionArmed = !repositionSelectionArmed;
    }
    else if (!canMoveSelection)
    {
        ImGui::TextUnformatted("Select something to move");
        repositionSelectionArmed = false;
    }

    ImGui::Separator();
    Camera* activeEditorCamera = GetActiveEditorCamera();
    lightEditor.RenderToolbar(selection, activeEditorCamera->Position + activeEditorCamera->Orientation * 35.0f, dirty);
    if (lightEditor.ConsumedNewSelection())
    {
        gizmoSystem.SetTranslateMode();
        lightEditor.SyncSelection(selection);
    }

    if (repositionSelectionArmed)
    {
        ImGui::TextUnformatted("Move Selected active: click once inside Editor Viewport to relocate the selection. Right click cancels.");
    }
    ImGui::TextUnformatted("Move selection: arrows on X/Z, PageUp/PageDown or U/O on height. Shift = faster, Ctrl = finer.");

    ImGui::Separator();
    gizmoSystem.DrawToolbar();
    ImGui::End();
}

void Editor::RenderSceneToFramebuffer(const ImVec2& viewportSize)
{
    const int targetWidth = std::max(static_cast<int>(viewportSize.x), 1);
    const int targetHeight = std::max(static_cast<int>(viewportSize.y), 1);
    EnsureFramebuffer(targetWidth, targetHeight);
    if (!framebufferReady)
    {
        return;
    }

    Camera* activeViewportCamera = viewportCameraControlActive && viewportCamera
        ? viewportCamera.get()
        : config.camera;

    viewportState.request.width = targetWidth;
    viewportState.request.height = targetHeight;
    viewportState.request.cameraPosition = activeViewportCamera->Position;
    viewportState.request.view = activeViewportCamera->GetViewMatrix();
    viewportState.request.projection = glm::perspective(
        glm::radians(config.cameraFov),
        static_cast<float>(targetWidth) / static_cast<float>(targetHeight),
        config.nearPlane,
        config.farPlane);

    activeViewportCamera->width = targetWidth;
    activeViewportCamera->height = targetHeight;

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, targetWidth, targetHeight);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.09f, 0.11f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (config.viewportRenderer)
    {
        config.viewportRenderer(viewportState.request);
    }

    RenderEditorHelpers(viewportState.request);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    int windowWidth = 0;
    int windowHeight = 0;
    glfwGetFramebufferSize(config.window, &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);
}

void Editor::RenderEditorHelpers(const EditorViewportRenderRequest& request)
{
    lightEditor.RenderViewportHelpers(request);
}

void Editor::TryPickViewport()
{
    const ImVec2 mousePosition = ImGui::GetMousePos();
    selection = pickingSystem.Pick(
        *config.scene,
        PickingViewport{
            glm::vec2(mousePosition.x - viewportState.origin.x, mousePosition.y - viewportState.origin.y),
            glm::vec2(viewportState.size.x, viewportState.size.y),
            viewportState.request.view,
            viewportState.request.projection,
            viewportState.request.cameraPosition });
    lightEditor.SyncSelection(selection);
}

bool Editor::MoveSelectedToViewportPoint(const ImVec2& mousePosition)
{
    glm::vec3* selectedPosition = GetSelectedPosition();
    if (!selectedPosition)
    {
        return false;
    }

    glm::vec3 intersection(0.0f);
    if (!ComputeViewportPlaneIntersection(mousePosition, 0.0f, intersection))
    {
        return false;
    }

    if (selection.type == SceneObjectType::Light)
    {
        *selectedPosition = intersection;
    }
    else
    {
        selectedPosition->x = intersection.x;
        selectedPosition->z = intersection.z;
    }

    dirty = true;
    lightEditor.SyncSelection(selection);
    return true;
}

bool Editor::ComputeViewportPlaneIntersection(const ImVec2& mousePosition, float planeY, glm::vec3& intersection) const
{
    const float localX = mousePosition.x - viewportState.origin.x;
    const float localY = mousePosition.y - viewportState.origin.y;
    const float ndcX = (2.0f * localX / viewportState.size.x) - 1.0f;
    const float ndcY = 1.0f - (2.0f * localY / viewportState.size.y);

    const glm::mat4 inverseViewProjection = glm::inverse(viewportState.request.projection * viewportState.request.view);
    glm::vec4 nearPoint = inverseViewProjection * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farPoint = inverseViewProjection * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;

    const glm::vec3 rayOrigin = glm::vec3(nearPoint);
    const glm::vec3 rayDirection = glm::normalize(glm::vec3(farPoint - nearPoint));
    if (std::abs(rayDirection.y) <= 0.0001f)
    {
        intersection = viewportState.request.cameraPosition + rayDirection * 60.0f;
        intersection.y = planeY;
        return true;
    }

    const float t = (planeY - rayOrigin.y) / rayDirection.y;
    if (t <= 0.0f)
    {
        intersection = viewportState.request.cameraPosition + rayDirection * 60.0f;
        intersection.y = planeY;
        return true;
    }

    intersection = rayOrigin + rayDirection * t;
    return true;
}

glm::vec3* Editor::GetSelectedPosition()
{
    if (!HasValidSelection())
    {
        return nullptr;
    }

    switch (selection.type)
    {
    case SceneObjectType::Entity:
        return &config.scene->entities[selection.index].position;
    case SceneObjectType::Light:
        return &config.scene->lights[selection.index].position;
    case SceneObjectType::Helper:
        return &config.scene->helpers[selection.index].position;
    default:
        return nullptr;
    }
}

void Editor::HandleSelectionKeyboardMove(float deltaTime)
{
    if (!HasValidSelection())
    {
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsAnyItemActive())
    {
        return;
    }

    glm::vec3* selectedPosition = GetSelectedPosition();
    if (!selectedPosition)
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

    *selectedPosition += movement;
    dirty = true;
    lightEditor.SyncSelection(selection);
}

void Editor::SaveChanges()
{
    const std::string sceneTempPath = MakeTempFilePath(config.sceneFilePath);
    const std::string lightTempPath = MakeTempFilePath(config.lightFilePath);

    const bool sceneSaved = serializer.Save(sceneTempPath, *config.scene);
    const bool lightsSaved = lightEditor.SaveToFile(lightTempPath);

    if (sceneSaved && lightsSaved &&
        CommitStagedFiles(sceneTempPath, config.sceneFilePath, lightTempPath, config.lightFilePath))
    {
        lightEditor.CommitSavedState();
        dirty = false;
    }
    else
    {
        std::error_code ec;
        fs::remove(sceneTempPath, ec);
        fs::remove(lightTempPath, ec);
    }
}

void Editor::RevertChanges()
{
    serializer.Load(config.sceneFilePath, *config.scene);
    lightEditor.Revert(config.scene->lights, selection);
    SyncHelperSequence();
    if (!IsSelectionInsideBounds(selection))
    {
        selection = {};
    }

    dirty = false;
}

void Editor::AddHelper()
{
    ++helperSequence;
    Helper helper;
    helper.id = "helper_" + std::to_string(helperSequence);
    helper.name = "Helper " + std::to_string(helperSequence);
    helper.position = config.camera->Position + config.camera->Orientation * 60.0f;
    config.scene->helpers.push_back(helper);
    selection = { SceneObjectType::Helper, static_cast<int>(config.scene->helpers.size()) - 1 };
    dirty = true;
}

void Editor::SyncHelperSequence()
{
    helperSequence = 0;
    for (const Helper& helper : config.scene->helpers)
    {
        helperSequence = std::max(helperSequence, ParseTrailingSequence(helper.id, "helper_"));
    }
}

int Editor::ParseTrailingSequence(const std::string& value, const char* prefix) const
{
    const std::string prefixValue(prefix);
    if (value.rfind(prefixValue, 0) != 0)
    {
        return 0;
    }

    const std::string suffix = value.substr(prefixValue.size());
    if (suffix.empty())
    {
        return 0;
    }

    std::int32_t parsedValue = 0;
    for (char character : suffix)
    {
        if (character < '0' || character > '9')
        {
            return 0;
        }

        parsedValue = (parsedValue * 10) + static_cast<std::int32_t>(character - '0');
    }

    return static_cast<int>(parsedValue);
}

bool Editor::HasValidSelection() const
{
    return IsSelectionInsideBounds(selection);
}

bool Editor::IsSelectionInsideBounds(const SceneSelection& currentSelection) const
{
    if (!currentSelection.IsValid())
    {
        return false;
    }

    switch (currentSelection.type)
    {
    case SceneObjectType::Entity:
        return currentSelection.index < static_cast<int>(config.scene->entities.size());
    case SceneObjectType::Light:
        return currentSelection.index < static_cast<int>(config.scene->lights.size());
    case SceneObjectType::Helper:
        return currentSelection.index < static_cast<int>(config.scene->helpers.size());
    default:
        return false;
    }
}

Camera* Editor::GetActiveEditorCamera() const
{
    return viewportCameraControlActive && viewportCamera
        ? viewportCamera.get()
        : config.camera;
}
