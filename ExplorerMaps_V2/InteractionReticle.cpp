#include "InteractionReticle.h"

#include "imgui/imgui.h"

namespace InteractionReticle {
    void DrawCenterDot(float screenWidth, float screenHeight, bool interactPressed) {
        // Aqui se dibuja el punto del centro de pantalla para interaccion.
        ImDrawList* drawList = ImGui::GetOverlayDrawList();
        const ImGuiIO& io = ImGui::GetIO();
        const float width = io.DisplaySize.x > 0.0f ? io.DisplaySize.x : screenWidth;
        const float height = io.DisplaySize.y > 0.0f ? io.DisplaySize.y : screenHeight;
        const ImVec2 center(width * 0.5f, height * 0.5f);
        const ImU32 shadowColor = IM_COL32(0, 0, 0, 140);
        const ImU32 dotColor = interactPressed ? IM_COL32(255, 210, 90, 255) : IM_COL32(255, 255, 255, 230);

        drawList->AddCircleFilled(center, interactPressed ? 5.5f : 4.0f, shadowColor, 16);
        drawList->AddCircleFilled(center, interactPressed ? 3.0f : 2.0f, dotColor, 16);
    }
}
