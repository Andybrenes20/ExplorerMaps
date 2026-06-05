#include "LightEditor.h"

#include <cmath>
#include <glm/gtc/matrix_inverse.hpp>
#include <imgui.h>

#include "Editor.h"

void LightEditor::Init(const LightEditorConfig& initConfig)
{
    config = initConfig;
    manager.Bind(config.lights);
    serializer.Load(config.lightFilePath, *config.lights);
    renderer.Init();
    savedLights = *config.lights;
}

void LightEditor::Shutdown()
{
    renderer.Shutdown();
}

void LightEditor::Update(const glm::vec3& spawnPosition, SceneSelection& selection, bool& dirty)
{
    manager.SyncSelection(selection);
    manager.HandleShortcuts(config.window, spawnPosition, selection, dirty);

    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard || ImGui::IsAnyItemActive())
    {
        shortcutCreateLatch = false;
        manager.SyncSelection(selection);
        return;
    }

    const bool ctrlDown = glfwGetKey(config.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
        glfwGetKey(config.window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
    const bool shiftDown = glfwGetKey(config.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(config.window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    const bool lDown = glfwGetKey(config.window, GLFW_KEY_L) == GLFW_PRESS;
    const bool quickCreatePressed = ctrlDown && shiftDown && lDown;
    if (quickCreatePressed && !shortcutCreateLatch)
    {
        selection = { SceneObjectType::Light, manager.CreatePointLight(spawnPosition) };
        dirty = true;
        newSelectionCreated = true;
    }
    shortcutCreateLatch = quickCreatePressed;

    manager.SyncSelection(selection);
}

void LightEditor::RenderHierarchy(const glm::vec3& spawnPosition, SceneSelection& selection, bool& dirty)
{
    const std::size_t before = config.lights->size();
    manager.DrawHierarchy(spawnPosition, selection);
    manager.SyncSelection(selection);
    const bool countChanged = before != config.lights->size();
    dirty |= countChanged;
    if (countChanged)
    {
        newSelectionCreated = true;
    }
}

void LightEditor::RenderInspector(SceneSelection& selection, bool& dirty)
{
    manager.DrawInspector(selection, dirty);
}

void LightEditor::RenderToolbar(SceneSelection& selection, const glm::vec3& spawnPosition, bool& dirty)
{
    if (ImGui::Button("Lamp Here"))
    {
        selection = { SceneObjectType::Light, manager.CreatePointLight(spawnPosition) };
        dirty = true;
        newSelectionCreated = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Area Here"))
    {
        selection = { SceneObjectType::Light, manager.CreateAreaLight(spawnPosition) };
        dirty = true;
        newSelectionCreated = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Cube Here"))
    {
        selection = { SceneObjectType::Light, manager.CreateCubeLight(spawnPosition) };
        dirty = true;
        newSelectionCreated = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Place Lamp"))
    {
        placementMode = PlacementMode::PointLight;
    }

    ImGui::SameLine();
    if (ImGui::Button("Place Area"))
    {
        placementMode = PlacementMode::AreaLight;
    }

    ImGui::SameLine();
    if (ImGui::Button("Place Cube"))
    {
        placementMode = PlacementMode::CubeLight;
    }

    ImGui::SameLine();
    if (ImGui::Button("Duplicate selected") && manager.HasLightSelection(selection))
    {
        const int duplicateIndex = manager.DuplicateSelected(selection);
        if (duplicateIndex >= 0)
        {
            selection = { SceneObjectType::Light, duplicateIndex };
            dirty = true;
            newSelectionCreated = true;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Delete selected") && manager.HasLightSelection(selection))
    {
        dirty |= manager.DeleteSelected(selection);
    }

    if (placementMode != PlacementMode::None)
    {
        if (placementMode != PlacementMode::MoveSelected)
        {
            ImGui::TextUnformatted("Click in the viewport to place the light. Right click cancels placement.");
        }
        else
        {
            ImGui::TextUnformatted("Click in the viewport to reposition the selected light. Right click cancels.");
        }
    }
    else
    {
        ImGui::TextUnformatted("Ctrl+Shift+L creates a lamp in front of the camera. Ctrl+D duplicates the selected light.");
    }
}

void LightEditor::RenderViewportHelpers(const EditorViewportRenderRequest& request) const
{
    renderer.Render(*config.lights, request);
}

bool LightEditor::Save()
{
    if (!SaveToFile(config.lightFilePath))
    {
        return false;
    }

    CommitSavedState();
    return true;
}

bool LightEditor::SaveToFile(const std::string& filePath) const
{
    return serializer.Save(filePath, *config.lights);
}

void LightEditor::CommitSavedState()
{
    savedLights = *config.lights;
}

void LightEditor::Revert(std::vector<Light>& lights, SceneSelection& selection)
{
    lights = savedLights;
    manager.Bind(&lights);
    manager.SyncSelection(selection);
}

void LightEditor::SyncSelection(SceneSelection& selection)
{
    manager.SyncSelection(selection);
}

bool LightEditor::HandleViewportClick(const ImVec2& mousePosition, const ImVec2& viewportOrigin, const ImVec2& viewportSize, const EditorViewportRenderRequest& request, SceneSelection& selection, bool& dirty)
{
    if (placementMode == PlacementMode::None)
    {
        return false;
    }

    const glm::vec3 placementPosition = ComputePlacementPosition(mousePosition, viewportOrigin, viewportSize, request);
    if (placementMode == PlacementMode::MoveSelected)
    {
        Light* selectedLight = manager.GetSelectedLight(selection);
        if (!selectedLight)
        {
            placementMode = PlacementMode::None;
            return false;
        }

        selectedLight->position = placementPosition;
        dirty = true;
    }
    else
    {
        selection = { SceneObjectType::Light, CreateRequestedLight(placementPosition) };
        dirty = true;
        newSelectionCreated = true;
    }

    placementMode = PlacementMode::None;
    return true;
}

bool LightEditor::IsPlacementActive() const
{
    return placementMode != PlacementMode::None;
}

bool LightEditor::ConsumedNewSelection()
{
    const bool wasCreated = newSelectionCreated;
    newSelectionCreated = false;
    return wasCreated;
}

void LightEditor::CancelPlacement()
{
    placementMode = PlacementMode::None;
}

glm::vec3 LightEditor::ComputePlacementPosition(const ImVec2& mousePosition, const ImVec2& viewportOrigin, const ImVec2& viewportSize, const EditorViewportRenderRequest& request) const
{
    const float localX = mousePosition.x - viewportOrigin.x;
    const float localY = mousePosition.y - viewportOrigin.y;
    const float ndcX = (2.0f * localX / viewportSize.x) - 1.0f;
    const float ndcY = 1.0f - (2.0f * localY / viewportSize.y);

    const glm::mat4 inverseViewProjection = glm::inverse(request.projection * request.view);
    glm::vec4 nearPoint = inverseViewProjection * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farPoint = inverseViewProjection * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;

    const glm::vec3 rayOrigin = glm::vec3(nearPoint);
    const glm::vec3 rayDirection = glm::normalize(glm::vec3(farPoint - nearPoint));

    const float planeY = 0.0f;
    if (std::abs(rayDirection.y) > 0.0001f)
    {
        const float t = (planeY - rayOrigin.y) / rayDirection.y;
        if (t > 0.0f)
        {
            return rayOrigin + rayDirection * t;
        }
    }

    return request.cameraPosition + rayDirection * 60.0f;
}

int LightEditor::CreateRequestedLight(const glm::vec3& position)
{
    if (placementMode == PlacementMode::AreaLight)
    {
        return manager.CreateAreaLight(position);
    }

    if (placementMode == PlacementMode::CubeLight)
    {
        return manager.CreateCubeLight(position);
    }

    return manager.CreatePointLight(position);
}
