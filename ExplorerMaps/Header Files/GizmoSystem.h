#pragma once

#include <glm/glm.hpp>
#include <imgui.h>

#include "EditorTypes.h"

namespace ImGuizmo
{
    enum OPERATION;
    enum MODE;
}

struct GizmoViewport
{
    ImVec2 origin = ImVec2(0.0f, 0.0f);
    ImVec2 size = ImVec2(1.0f, 1.0f);
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);
};

class GizmoSystem
{
public:
    void HandleShortcuts();
    bool Draw(EditorSceneData& scene, const SceneSelection& selection, const GizmoViewport& viewport);
    void DrawToolbar();
    bool IsUsing() const;
    bool IsOver() const;
    void SetTranslateMode();
    ImGuizmo::OPERATION GetCurrentOperation() const;

private:
    int currentOperation = 0;
    int currentMode = 0;
    bool useSnap = false;
    glm::vec3 translationSnap = glm::vec3(10.0f);
    float rotationSnap = 15.0f;
    float scaleSnap = 0.25f;
    bool usingGizmo = false;
};
