#include "GizmoSystem.h"

#include <algorithm>

#include "ImGuizmo.h"

namespace
{
    glm::mat4 GetSelectionMatrix(const EditorSceneData& scene, const SceneSelection& selection)
    {
        switch (selection.type)
        {
        case SceneObjectType::Entity:
            return ComposeEntityMatrix(scene.entities.at(selection.index));
        case SceneObjectType::Light:
            return ComposeLightMatrix(scene.lights.at(selection.index));
        case SceneObjectType::Helper:
            return ComposeHelperMatrix(scene.helpers.at(selection.index));
        default:
            return glm::mat4(1.0f);
        }
    }

    void ApplyMatrix(EditorSceneData& scene, const SceneSelection& selection, const glm::mat4& matrix)
    {
        float translation[3] = {};
        float rotation[3] = {};
        float scale[3] = {};
        ImGuizmo::DecomposeMatrixToComponents(&matrix[0][0], translation, rotation, scale);

        if (selection.type == SceneObjectType::Entity)
        {
            Entity& entity = scene.entities.at(selection.index);
            entity.position = glm::vec3(translation[0], translation[1], translation[2]);
            entity.rotation = glm::vec3(rotation[0], rotation[1], rotation[2]);
            entity.scale = glm::vec3(
                std::max(scale[0], 0.01f),
                std::max(scale[1], 0.01f),
                std::max(scale[2], 0.01f));
        }
        else if (selection.type == SceneObjectType::Helper)
        {
            Helper& helper = scene.helpers.at(selection.index);
            helper.position = glm::vec3(translation[0], translation[1], translation[2]);
            helper.rotation = glm::vec3(rotation[0], rotation[1], rotation[2]);
            helper.scale = glm::vec3(
                std::max(scale[0], 0.1f),
                std::max(scale[1], 0.1f),
                std::max(scale[2], 0.1f));
        }
        else if (selection.type == SceneObjectType::Light)
        {
            Light& light = scene.lights.at(selection.index);
            light.position = glm::vec3(translation[0], translation[1], translation[2]);
            const float averageScale = std::max((scale[0] + scale[1] + scale[2]) / 3.0f, 1.0f);
            light.radius = averageScale;
            light.helperSize = std::max(averageScale * 0.2f, 1.0f);
        }
    }
}

void GizmoSystem::HandleShortcuts()
{
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard || ImGui::IsAnyItemActive())
    {
        return;
    }

    if (ImGui::IsKeyPressed('W', false))
    {
        currentOperation = static_cast<int>(ImGuizmo::TRANSLATE);
    }

    if (ImGui::IsKeyPressed('E', false))
    {
        currentOperation = static_cast<int>(ImGuizmo::ROTATE);
    }

    if (ImGui::IsKeyPressed('R', false))
    {
        currentOperation = static_cast<int>(ImGuizmo::SCALE);
    }
}

bool GizmoSystem::Draw(EditorSceneData& scene, const SceneSelection& selection, const GizmoViewport& viewport)
{
    if (!selection.IsValid())
    {
        usingGizmo = false;
        return false;
    }

    ImGuizmo::BeginFrame();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(viewport.origin.x, viewport.origin.y, viewport.size.x, viewport.size.y);

    glm::mat4 matrix = GetSelectionMatrix(scene, selection);
    const ImGuizmo::OPERATION operation = (selection.type == SceneObjectType::Light &&
        currentOperation == static_cast<int>(ImGuizmo::ROTATE))
        ? ImGuizmo::TRANSLATE
        : static_cast<ImGuizmo::OPERATION>(currentOperation);
    const ImGuizmo::MODE mode = currentOperation == static_cast<int>(ImGuizmo::SCALE)
        ? ImGuizmo::LOCAL
        : static_cast<ImGuizmo::MODE>(currentMode);

    float snapValues[3] =
    {
        currentOperation == static_cast<int>(ImGuizmo::ROTATE) ? rotationSnap : translationSnap.x,
        currentOperation == static_cast<int>(ImGuizmo::ROTATE) ? rotationSnap : translationSnap.y,
        currentOperation == static_cast<int>(ImGuizmo::ROTATE) ? rotationSnap : translationSnap.z
    };

    if (currentOperation == static_cast<int>(ImGuizmo::SCALE))
    {
        snapValues[0] = scaleSnap;
        snapValues[1] = scaleSnap;
        snapValues[2] = scaleSnap;
    }

    ImGuizmo::Manipulate(
        &viewport.view[0][0],
        &viewport.projection[0][0],
        operation,
        mode,
        &matrix[0][0],
        nullptr,
        useSnap ? snapValues : nullptr);

    usingGizmo = ImGuizmo::IsUsing();
    if (usingGizmo)
    {
        ApplyMatrix(scene, selection, matrix);
    }

    return usingGizmo;
}

void GizmoSystem::DrawToolbar()
{
    ImGui::TextUnformatted("Gizmo");
    ImGui::SameLine();
    if (ImGui::RadioButton("Move", currentOperation == static_cast<int>(ImGuizmo::TRANSLATE)))
    {
        currentOperation = static_cast<int>(ImGuizmo::TRANSLATE);
    }

    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", currentOperation == static_cast<int>(ImGuizmo::ROTATE)))
    {
        currentOperation = static_cast<int>(ImGuizmo::ROTATE);
    }

    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", currentOperation == static_cast<int>(ImGuizmo::SCALE)))
    {
        currentOperation = static_cast<int>(ImGuizmo::SCALE);
    }

    ImGui::SameLine();
    ImGui::Checkbox("Snap", &useSnap);

    if (currentOperation != static_cast<int>(ImGuizmo::SCALE))
    {
        if (ImGui::RadioButton("Local", currentMode == static_cast<int>(ImGuizmo::LOCAL)))
        {
            currentMode = static_cast<int>(ImGuizmo::LOCAL);
        }

        ImGui::SameLine();
        if (ImGui::RadioButton("World", currentMode == static_cast<int>(ImGuizmo::WORLD)))
        {
            currentMode = static_cast<int>(ImGuizmo::WORLD);
        }
    }

    if (currentOperation == static_cast<int>(ImGuizmo::TRANSLATE))
    {
        ImGui::DragFloat3("Move snap", &translationSnap.x, 0.25f, 0.1f, 500.0f, "%.2f");
    }
    else if (currentOperation == static_cast<int>(ImGuizmo::ROTATE))
    {
        ImGui::DragFloat("Rotate snap", &rotationSnap, 0.5f, 1.0f, 180.0f, "%.1f");
    }
    else
    {
        ImGui::DragFloat("Scale snap", &scaleSnap, 0.01f, 0.01f, 5.0f, "%.2f");
    }
}

bool GizmoSystem::IsUsing() const
{
    return usingGizmo;
}

bool GizmoSystem::IsOver() const
{
    return ImGuizmo::IsOver();
}

void GizmoSystem::SetTranslateMode()
{
    currentOperation = static_cast<int>(ImGuizmo::TRANSLATE);
}

ImGuizmo::OPERATION GizmoSystem::GetCurrentOperation() const
{
    return static_cast<ImGuizmo::OPERATION>(currentOperation);
}
