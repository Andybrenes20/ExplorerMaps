#include "LoadingScreen.h"

#include <algorithm>
#include <cmath>

#include "imgui/imgui.h"
#include "Localization.h"

void LoadingScreen::Initialize() {
    ImGuiIO& io = ImGui::GetIO();
    titleFont = io.Fonts->AddFontFromFileTTF("Texturas/Fonts/calibri.ttf", 48.0f);
    bodyFont = io.Fonts->AddFontFromFileTTF("Texturas/Fonts/calibri.ttf", 20.0f);
}

void LoadingScreen::Draw(float progress, float currentTime) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float width = std::min(display.x * 0.62f, 720.0f);
    const float barHeight = 18.0f;
    const float centerX = display.x * 0.5f;
    const float titleY = display.y * 0.40f;
    const float barY = display.y * 0.57f;
    const float targetProgress = std::clamp(progress, 0.0f, 1.0f);
    displayedProgress += (targetProgress - displayedProgress) * std::clamp(ImGui::GetIO().DeltaTime * 12.0f, 0.0f, 1.0f);
    if (targetProgress >= 1.0f) {
        displayedProgress = 1.0f;
    }

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display, ImGuiCond_Always);
    ImGui::Begin(
        "LoadingScreen",
        nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(8, 10, 14, 255));

    const char* title = Localization::Text("CARGANDO EXPLORERMAPS", "LOADING EXPLORERMAPS");
    ImGui::PushFont(titleFont);
    const ImVec2 titleSize = ImGui::CalcTextSize(title);
    drawList->AddText(
        titleFont,
        titleFont->FontSize,
        ImVec2(centerX - titleSize.x * 0.5f, titleY),
        IM_COL32(242, 185, 0, 255),
        title);
    ImGui::PopFont();

    const ImVec2 barMin(centerX - width * 0.5f, barY);
    const ImVec2 barMax(centerX + width * 0.5f, barY + barHeight);
    drawList->AddRectFilled(barMin, barMax, IM_COL32(35, 44, 58, 255), 4.0f);
    drawList->AddRectFilled(barMin, ImVec2(barMin.x + width * displayedProgress, barMax.y), IM_COL32(0, 92, 205, 255), 4.0f);
    drawList->AddRect(barMin, barMax, IM_COL32(242, 185, 0, 220), 4.0f, 15, 2.0f);

    const char* status = Localization::Text("PREPARANDO EL MAPA...", "PREPARING THE MAP...");
    ImGui::PushFont(bodyFont);
    const ImVec2 statusSize = ImGui::CalcTextSize(status);
    drawList->AddText(
        bodyFont,
        bodyFont->FontSize,
        ImVec2(centerX - statusSize.x * 0.5f, barMax.y + 24.0f),
        IM_COL32(224, 230, 238, 255),
        status);
    ImGui::PopFont();

    ImGui::End();
}

void LoadingScreen::DrawReturnToMenu(float progress) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float width = std::min(display.x * 0.56f, 660.0f);
    const float centerX = display.x * 0.5f;
    const float titleY = display.y * 0.42f;
    const float barY = display.y * 0.57f;

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display, ImGuiCond_Always);
    ImGui::Begin(
        "ReturnToMainMenuLoading",
        nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(8, 10, 14, 255));
    const char* title = Localization::Text("REGRESANDO AL MENU", "RETURNING TO MAIN MENU");
    ImGui::PushFont(titleFont);
    const ImVec2 titleSize = ImGui::CalcTextSize(title);
    drawList->AddText(titleFont, titleFont->FontSize, ImVec2(centerX - titleSize.x * 0.5f, titleY), IM_COL32(242, 185, 0, 255), title);
    ImGui::PopFont();

    const ImVec2 barMin(centerX - width * 0.5f, barY);
    const ImVec2 barMax(centerX + width * 0.5f, barY + 18.0f);
    drawList->AddRectFilled(barMin, barMax, IM_COL32(35, 44, 58, 255), 4.0f);
    drawList->AddRectFilled(barMin, ImVec2(barMin.x + width * std::clamp(progress, 0.0f, 1.0f), barMax.y), IM_COL32(0, 92, 205, 255), 4.0f);
    drawList->AddRect(barMin, barMax, IM_COL32(242, 185, 0, 220), 4.0f, 15, 2.0f);
    ImGui::End();
}
