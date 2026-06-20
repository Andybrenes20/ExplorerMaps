#include "MainMenu.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "imgui/imgui.h"
#include "stb/stb_image.h"
#include "EnvironmentSystem.h"
#include "Localization.h"

namespace {
    constexpr float kSlideSeconds = 6.0f;
    constexpr float kFadeSeconds = 1.25f;

    ImVec2 ScaleToDisplay(float x, float y, const ImVec2& display) {
        return ImVec2(x * display.x / 1280.0f, y * display.y / 720.0f);
    }

    void DrawCoveredImage(ImDrawList* drawList, const MainMenu::Background& image, const ImVec2& display, float alpha) {
        const float displayAspect = display.x / (std::max)(display.y, 1.0f);
        const float imageAspect = static_cast<float>(image.width) / (std::max)(static_cast<float>(image.height), 1.0f);

        ImVec2 uvMin(0.0f, 0.0f);
        ImVec2 uvMax(1.0f, 1.0f);
        if (imageAspect > displayAspect) {
            const float visible = displayAspect / imageAspect;
            const float margin = (1.0f - visible) * 0.5f;
            uvMin.x = margin;
            uvMax.x = 1.0f - margin;
        }
        else {
            const float visible = imageAspect / displayAspect;
            const float margin = (1.0f - visible) * 0.5f;
            uvMin.y = margin;
            uvMax.y = 1.0f - margin;
        }

        drawList->AddImage(
            reinterpret_cast<ImTextureID>(static_cast<intptr_t>(image.texture)),
            ImVec2(0.0f, 0.0f),
            display,
            uvMin,
            uvMax,
            IM_COL32(255, 255, 255, static_cast<int>(255.0f * alpha)));
    }
}

bool MainMenu::Initialize() {
    ImGuiIO& io = ImGui::GetIO();
    titleFont = io.Fonts->AddFontFromFileTTF("Texturas/Fonts/calibri.ttf", 62.0f);
    buttonFont = io.Fonts->AddFontFromFileTTF("Texturas/Fonts/calibri.ttf", 30.0f);
    bodyFont = io.Fonts->AddFontFromFileTTF("Texturas/Fonts/calibri.ttf", 22.0f);

    const char* paths[] = {
        "Image/Menu/Image_1.jpeg",
        "Image/Menu/Image_2.jpeg",
        "Image/Menu/Image_3.jpeg",
        "Image/Menu/Image_4.jpeg",
        "Image/Menu/Image_5.jpeg"
    };
    for (const char* path : paths) {
        LoadBackground(path);
    }
    LoadTexture("Image/Menu/Image_6.png", logo);
    LoadTexture("Image/Pause/Menu_Pause.png", pauseMenu);
    LoadTexture("Image/Pause/Atmosphere.png", atmosphereMenu);
    LoadTexture("Image/Pause/Dualsense.png", dualsense);
    return !backgrounds.empty();
}

void MainMenu::SetButtonSoundCallback(std::function<void()> callback) {
    buttonSoundCallback = std::move(callback);
}

void MainMenu::PlayButtonSound() {
    if (buttonSoundCallback) {
        buttonSoundCallback();
    }
}

MainMenuAction MainMenu::Draw(float currentTime) {
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 display = io.DisplaySize;

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display, ImGuiCond_Always);
    ImGui::Begin(
        "MainMenu",
        nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings);

    DrawBackground(currentTime);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    const ImVec2 titleCenter(display.x * 0.5f, ScaleToDisplay(0.0f, 105.0f, display).y);
    const ImVec2 titlePanelSize = ScaleToDisplay(590.0f, 142.0f, display);
    const ImVec2 titleMin(titleCenter.x - titlePanelSize.x * 0.5f, titleCenter.y - titlePanelSize.y * 0.5f);
    const ImVec2 titleMax(titleCenter.x + titlePanelSize.x * 0.5f, titleCenter.y + titlePanelSize.y * 0.5f);
    drawList->AddRectFilled(ImVec2(titleMin.x + 8.0f, titleMin.y + 10.0f), ImVec2(titleMax.x + 8.0f, titleMax.y + 10.0f), IM_COL32(0, 0, 0, 120), 8.0f);
    drawList->AddRectFilled(titleMin, titleMax, IM_COL32(10, 20, 32, 242), 8.0f);
    drawList->AddRect(titleMin, titleMax, IM_COL32(242, 185, 0, 255), 8.0f, 15, 3.0f);
    drawList->AddLine(ImVec2(titleMin.x + 26.0f, titleMin.y + 14.0f), ImVec2(titleMax.x - 26.0f, titleMin.y + 14.0f), IM_COL32(255, 224, 88, 220), 2.0f);
    drawList->AddLine(ImVec2(titleMin.x + 55.0f, titleMax.y - 14.0f), ImVec2(titleMax.x - 55.0f, titleMax.y - 14.0f), IM_COL32(52, 143, 225, 190), 2.0f);

    const char* title = "EXPLORER MAPS";
    ImGui::PushFont(titleFont);
    const ImVec2 titleSize = ImGui::CalcTextSize(title);
    const ImVec2 titlePos(titleCenter.x - titleSize.x * 0.5f, titleCenter.y - titleSize.y * 0.5f);
    drawList->AddText(titleFont, titleFont->FontSize, ImVec2(titlePos.x + 4.0f, titlePos.y + 5.0f), IM_COL32(0, 0, 0, 155), title);
    drawList->AddText(titleFont, titleFont->FontSize, titlePos, IM_COL32(255, 196, 20, 255), title);
    ImGui::PopFont();

    MainMenuAction action = MainMenuAction::None;
    if (!creditsOpen && !optionsOpen) {
        UpdateControllerNavigation(4);
        const ImVec2 buttonSize = ScaleToDisplay(340.0f, 66.0f, display);
        if (DrawAngledButton("start", Localization::Text("INICIAR", "START"), ScaleToDisplay(640.0f, 255.0f, display), buttonSize, controllerNavigationActive && navigationSelection == 0) || ActivateSelection(0)) {
            action = MainMenuAction::Start;
        }
        if (DrawAngledButton("credits", Localization::Text("CREDITOS", "CREDITS"), ScaleToDisplay(640.0f, 345.0f, display), buttonSize, controllerNavigationActive && navigationSelection == 1) || ActivateSelection(1)) {
            creditsOpen = true;
            navigationSelection = 0;
        }
        if (DrawAngledButton("options", Localization::Text("OPCIONES", "OPTIONS"), ScaleToDisplay(640.0f, 435.0f, display), buttonSize, controllerNavigationActive && navigationSelection == 2) || ActivateSelection(2)) {
            optionsOpen = true;
            optionsPage = OptionsPage::Main;
            navigationSelection = 0;
        }
        if (DrawAngledButton("quit", Localization::Text("CERRAR", "QUIT"), ScaleToDisplay(640.0f, 525.0f, display), buttonSize, controllerNavigationActive && navigationSelection == 3) || ActivateSelection(3)) {
            action = MainMenuAction::Quit;
        }
    }
    else if (creditsOpen) {
        UpdateControllerNavigation(1);
        DrawCredits();
        if (navAcceptPressed) {
            PlayButtonSound();
            creditsOpen = false;
        }
        else if (navBackPressed) {
            creditsOpen = false;
        }
    }
    else if (DrawOptions()) {
        optionsOpen = false;
        optionsPage = OptionsPage::Main;
        navigationSelection = 0;
    }

    ImGui::End();
    return action;
}

MainMenuAction MainMenu::DrawPause(EnvironmentSystem& environment) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display, ImGuiCond_Always);
    ImGui::Begin(
        "PauseMenu",
        nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(0, 0, 0, 145));
    ImVec2 panelMin;
    ImVec2 panelMax;
    MainMenuAction action = MainMenuAction::None;

    if (pausePage == PausePage::Atmosphere) {
        UpdateControllerNavigation(4);
        DrawTechPanel(Localization::Text("AMBIENTE", "ENVIRONMENT"), panelMin, panelMax);
        const float w = panelMax.x - panelMin.x;
        const float h = panelMax.y - panelMin.y;
        const ImVec2 buttonSize(w * 0.62f, h * 0.105f);
        if (DrawMenuButton("environment_day", Localization::Text("DIA", "DAY"), ImVec2((panelMin.x + panelMax.x) * 0.5f, panelMin.y + h * 0.32f), buttonSize, environment.GetMode() == EnvironmentMode::Day || (controllerNavigationActive && navigationSelection == 0)) || ActivateSelection(0)) {
            environment.SetMode(EnvironmentMode::Day);
        }
        if (DrawMenuButton("environment_night", Localization::Text("NOCHE", "NIGHT"), ImVec2((panelMin.x + panelMax.x) * 0.5f, panelMin.y + h * 0.47f), buttonSize, environment.GetMode() == EnvironmentMode::Night || (controllerNavigationActive && navigationSelection == 1)) || ActivateSelection(1)) {
            environment.SetMode(EnvironmentMode::Night);
        }
        if (DrawMenuButton("environment_auto", Localization::Text("AUTOMATICO", "AUTOMATIC"), ImVec2((panelMin.x + panelMax.x) * 0.5f, panelMin.y + h * 0.62f), buttonSize, environment.GetMode() == EnvironmentMode::Auto || (controllerNavigationActive && navigationSelection == 2)) || ActivateSelection(2)) {
            environment.SetMode(EnvironmentMode::Auto);
        }
        if (DrawMenuButton("environment_back", Localization::Text("VOLVER", "BACK"), ImVec2((panelMin.x + panelMax.x) * 0.5f, panelMin.y + h * 0.82f), ImVec2(w * 0.42f, h * 0.09f), controllerNavigationActive && navigationSelection == 3) || ActivateSelection(3) || navBackPressed) {
            pausePage = PausePage::Main;
            navigationSelection = 0;
        }
    }
    else if (pausePage == PausePage::Controls) {
        UpdateControllerNavigation(3, true);
        DrawTechPanel(Localization::Text("CONTROLES", "CONTROLS"), panelMin, panelMax, controlsPage == ControlsPage::Gamepad ? 0.76f : 0.58f);
        const float w = panelMax.x - panelMin.x;
        const float h = panelMax.y - panelMin.y;
        if (DrawMenuButton("keyboard_tab", Localization::Text("TECLADO", "KEYBOARD"), ImVec2(panelMin.x + w * 0.34f, panelMin.y + h * 0.22f), ImVec2(w * 0.28f, h * 0.075f), controlsPage == ControlsPage::Keyboard || (controllerNavigationActive && navigationSelection == 0)) || ActivateSelection(0)) {
            controlsPage = ControlsPage::Keyboard;
        }
        if (DrawMenuButton("gamepad_tab", Localization::Text("MANDO", "GAMEPAD"), ImVec2(panelMin.x + w * 0.66f, panelMin.y + h * 0.22f), ImVec2(w * 0.28f, h * 0.075f), controlsPage == ControlsPage::Gamepad || (controllerNavigationActive && navigationSelection == 1)) || ActivateSelection(1)) {
            controlsPage = ControlsPage::Gamepad;
        }
        if (controlsPage == ControlsPage::Keyboard) {
            DrawKeyboardControls(panelMin, panelMax);
        }
        else {
            DrawGamepadControls(panelMin, panelMax);
        }
        if (DrawMenuButton("controls_back", Localization::Text("VOLVER", "BACK"), ImVec2((panelMin.x + panelMax.x) * 0.5f, panelMin.y + h * 0.88f), ImVec2(w * 0.42f, h * 0.075f), controllerNavigationActive && navigationSelection == 2) || ActivateSelection(2) || navBackPressed) {
            pausePage = PausePage::Main;
            navigationSelection = 0;
        }
    }
    else if (pausePage == PausePage::Options) {
        if (DrawOptions()) {
            pausePage = PausePage::Main;
            optionsPage = OptionsPage::Main;
            navigationSelection = 0;
        }
    }
    else {
        UpdateControllerNavigation(5);
        DrawTechPanel(Localization::Text("PAUSA", "PAUSE"), panelMin, panelMax);
        const float w = panelMax.x - panelMin.x;
        const float h = panelMax.y - panelMin.y;
        const ImVec2 buttonSize(w * 0.66f, h * 0.10f);
        const float centerX = (panelMin.x + panelMax.x) * 0.5f;
        if (DrawMenuButton("pause_resume", Localization::Text("REANUDAR", "RESUME"), ImVec2(centerX, panelMin.y + h * 0.27f), buttonSize, controllerNavigationActive && navigationSelection == 0) || ActivateSelection(0) || navBackPressed) {
            pausePage = PausePage::Main;
            action = MainMenuAction::Resume;
        }
        if (DrawMenuButton("pause_atmosphere", Localization::Text("AMBIENTE", "ENVIRONMENT"), ImVec2(centerX, panelMin.y + h * 0.39f), buttonSize, controllerNavigationActive && navigationSelection == 1) || ActivateSelection(1)) {
            pausePage = PausePage::Atmosphere;
            navigationSelection = 0;
        }
        if (DrawMenuButton("pause_controls", Localization::Text("CONTROLES", "CONTROLS"), ImVec2(centerX, panelMin.y + h * 0.51f), buttonSize, controllerNavigationActive && navigationSelection == 2) || ActivateSelection(2)) {
            pausePage = PausePage::Controls;
            navigationSelection = 0;
        }
        if (DrawMenuButton("pause_options", Localization::Text("OPCIONES", "OPTIONS"), ImVec2(centerX, panelMin.y + h * 0.63f), buttonSize, controllerNavigationActive && navigationSelection == 3) || ActivateSelection(3)) {
            pausePage = PausePage::Options;
            optionsPage = OptionsPage::Main;
            navigationSelection = 0;
        }
        if (DrawMenuButton("pause_main_menu", Localization::Text("VOLVER AL MENU", "RETURN TO MAIN MENU"), ImVec2(centerX, panelMin.y + h * 0.78f), ImVec2(w * 0.78f, h * 0.10f), controllerNavigationActive && navigationSelection == 4) || ActivateSelection(4)) {
            pausePage = PausePage::Main;
            action = MainMenuAction::ReturnToMainMenu;
        }
    }

    ImGui::End();
    return action;
}

void MainMenu::DrawEnvironmentMenu(EnvironmentSystem& environment) {
    if (!environment.IsMenuOpen()) {
        return;
    }

    UpdateControllerNavigation(4);
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display, ImGuiCond_Always);
    ImGui::Begin(
        "EnvironmentImageMenu",
        nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(0, 0, 0, 135));
    ImVec2 panelMin;
    ImVec2 panelMax;
    DrawTechPanel(Localization::Text("AMBIENTE", "ENVIRONMENT"), panelMin, panelMax);
    const float w = panelMax.x - panelMin.x;
    const float h = panelMax.y - panelMin.y;
    const ImVec2 buttonSize(w * 0.62f, h * 0.105f);
    const float centerX = (panelMin.x + panelMax.x) * 0.5f;
    if (DrawMenuButton("game_environment_day", Localization::Text("DIA", "DAY"), ImVec2(centerX, panelMin.y + h * 0.32f), buttonSize, environment.GetMode() == EnvironmentMode::Day || (controllerNavigationActive && navigationSelection == 0)) || ActivateSelection(0)) {
        environment.SetMode(EnvironmentMode::Day);
        environment.CloseMenu();
    }
    if (DrawMenuButton("game_environment_night", Localization::Text("NOCHE", "NIGHT"), ImVec2(centerX, panelMin.y + h * 0.47f), buttonSize, environment.GetMode() == EnvironmentMode::Night || (controllerNavigationActive && navigationSelection == 1)) || ActivateSelection(1)) {
        environment.SetMode(EnvironmentMode::Night);
        environment.CloseMenu();
    }
    if (DrawMenuButton("game_environment_auto", Localization::Text("AUTOMATICO", "AUTOMATIC"), ImVec2(centerX, panelMin.y + h * 0.62f), buttonSize, environment.GetMode() == EnvironmentMode::Auto || (controllerNavigationActive && navigationSelection == 2)) || ActivateSelection(2)) {
        environment.SetMode(EnvironmentMode::Auto);
        environment.CloseMenu();
    }
    if (DrawMenuButton("game_environment_back", Localization::Text("VOLVER", "BACK"), ImVec2(centerX, panelMin.y + h * 0.82f), ImVec2(w * 0.42f, h * 0.09f), controllerNavigationActive && navigationSelection == 3) || ActivateSelection(3) || navBackPressed) {
        environment.CloseMenu();
    }
    ImGui::End();
}

void MainMenu::Shutdown() {
    for (const Background& background : backgrounds) {
        if (background.texture != 0) {
            glDeleteTextures(1, &background.texture);
        }
    }
    backgrounds.clear();
    if (logo.texture != 0) {
        glDeleteTextures(1, &logo.texture);
        logo = Background{};
    }
    if (pauseMenu.texture != 0) {
        glDeleteTextures(1, &pauseMenu.texture);
        pauseMenu = Background{};
    }
    if (atmosphereMenu.texture != 0) {
        glDeleteTextures(1, &atmosphereMenu.texture);
        atmosphereMenu = Background{};
    }
    if (dualsense.texture != 0) {
        glDeleteTextures(1, &dualsense.texture);
        dualsense = Background{};
    }
}

bool MainMenu::LoadBackground(const char* path) {
    Background background;
    if (!LoadTexture(path, background)) {
        return false;
    }
    backgrounds.push_back(background);
    return true;
}

bool MainMenu::LoadTexture(const char* path, Background& background) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* pixels = stbi_load(path, &width, &height, &channels, 4);
    if (!pixels) {
        return false;
    }

    background.width = width;
    background.height = height;
    glGenTextures(1, &background.texture);
    glBindTexture(GL_TEXTURE_2D, background.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixels);

    return true;
}

bool MainMenu::DrawAngledButton(const char* id, const char* label, const ImVec2& center, const ImVec2& size, bool selected) {
    const ImVec2 min(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
    ImGui::SetCursorScreenPos(min);
    const bool pressed = ImGui::InvisibleButton(id, size);
    if (pressed) {
        PlayButtonSound();
    }
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();

    const float slant = size.x * 0.035f;
    const ImVec2 p1(min.x + slant, min.y);
    const ImVec2 p2(min.x + size.x - slant, min.y - 4.0f);
    const ImVec2 p3(min.x + size.x, min.y + size.y - 5.0f);
    const ImVec2 p4(min.x, min.y + size.y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddQuadFilled(
        ImVec2(p1.x + 5.0f, p1.y + 7.0f),
        ImVec2(p2.x + 5.0f, p2.y + 7.0f),
        ImVec2(p3.x + 5.0f, p3.y + 7.0f),
        ImVec2(p4.x + 5.0f, p4.y + 7.0f),
        IM_COL32(0, 0, 0, 110));
    const ImU32 color = held
        ? IM_COL32(0, 64, 154, 255)
        : ((hovered || selected) ? IM_COL32(18, 105, 216, 255) : IM_COL32(0, 75, 180, 255));
    drawList->AddQuadFilled(p1, p2, p3, p4, color);

    ImGui::PushFont(buttonFont);
    const ImVec2 textSize = ImGui::CalcTextSize(label);
    drawList->AddText(
        buttonFont,
        buttonFont->FontSize,
        ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f - 2.0f),
        IM_COL32(4, 8, 14, 255),
        label);
    ImGui::PopFont();
    return pressed;
}

void MainMenu::DrawBackground(float currentTime) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    if (backgrounds.empty()) {
        drawList->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(30, 35, 45, 255));
        return;
    }

    const float slide = (std::max)(currentTime, 0.0f) / kSlideSeconds;
    const int current = static_cast<int>(slide) % static_cast<int>(backgrounds.size());
    const int next = (current + 1) % static_cast<int>(backgrounds.size());
    const float local = slide - std::floor(slide);
    const float fadeStart = 1.0f - kFadeSeconds / kSlideSeconds;
    const float fade = (std::clamp)((local - fadeStart) / (1.0f - fadeStart), 0.0f, 1.0f);

    DrawCoveredImage(drawList, backgrounds[current], display, 1.0f);
    if (fade > 0.0f) {
        DrawCoveredImage(drawList, backgrounds[next], display, fade);
    }
    drawList->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(6, 10, 18, 142));
}

void MainMenu::DrawCredits() {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 panelMin = ScaleToDisplay(350.0f, 215.0f, display);
    const ImVec2 panelMax = ScaleToDisplay(930.0f, 555.0f, display);
    drawList->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(0, 0, 0, 105));
    drawList->AddRectFilled(panelMin, panelMax, IM_COL32(13, 25, 43, 238), 6.0f);
    drawList->AddRect(panelMin, panelMax, IM_COL32(242, 185, 0, 255), 6.0f, 15, 3.0f);

    ImGui::PushFont(buttonFont);
    const char* heading = Localization::Text("CREDITOS", "CREDITS");
    const ImVec2 headingSize = ImGui::CalcTextSize(heading);
    drawList->AddText(
        ImVec2((display.x - headingSize.x) * 0.5f, panelMin.y + 28.0f),
        IM_COL32(242, 185, 0, 255),
        heading);
    ImGui::PopFont();

    ImGui::PushFont(bodyFont);
    const char* text = Localization::Text(
        "EXPLORER MAPS\nEquipo ExplorerMaps\n\nBrenes Ruiz Andy Antony\n\nOrtiz Vega Bianka Marcela\n\nVasquez Castillo Jeferson Evener",
        "EXPLORER MAPS\nExplorerMaps Team\n\nBrenes Ruiz Andy Antony\n\nOrtiz Vega Bianka Marcela\n\nVasquez Castillo Jeferson Evener");
    const ImVec2 textSize = ImGui::CalcTextSize(text);
    drawList->AddText(
        ImVec2((display.x - textSize.x) * 0.5f, panelMin.y + 98.0f),
        IM_COL32(235, 239, 245, 255),
        text);
    ImGui::PopFont();

    if (DrawAngledButton("back", Localization::Text("VOLVER", "BACK"), ScaleToDisplay(640.0f, 505.0f, display), ScaleToDisplay(260.0f, 58.0f, display))) {
        creditsOpen = false;
    }
}

bool MainMenu::DrawOptions() {
    if (optionsPage == OptionsPage::Language) {
        if (DrawLanguageSelector()) {
            optionsPage = OptionsPage::Main;
            navigationSelection = 0;
        }
        return false;
    }

    UpdateControllerNavigation(3);
    ImVec2 panelMin;
    ImVec2 panelMax;
    DrawTechPanel(Localization::Text("OPCIONES", "OPTIONS"), panelMin, panelMax);
    const float w = panelMax.x - panelMin.x;
    const float h = panelMax.y - panelMin.y;
    const float centerX = (panelMin.x + panelMax.x) * 0.5f;
    const ImVec2 buttonSize(w * 0.72f, h * 0.105f);
    const char* shadowsLabel = Localization::shadowsEnabled
        ? Localization::Text("SOMBRAS: ACTIVADAS", "SHADOWS: ON")
        : Localization::Text("SOMBRAS: DESACTIVADAS", "SHADOWS: OFF");

    if (DrawMenuButton("options_shadows", shadowsLabel, ImVec2(centerX, panelMin.y + h * 0.34f), buttonSize, controllerNavigationActive && navigationSelection == 0) || ActivateSelection(0)) {
        Localization::shadowsEnabled = !Localization::shadowsEnabled;
    }
    if (DrawMenuButton("options_language", Localization::Text("IDIOMA", "LANGUAGE"), ImVec2(centerX, panelMin.y + h * 0.52f), buttonSize, controllerNavigationActive && navigationSelection == 1) || ActivateSelection(1)) {
        optionsPage = OptionsPage::Language;
        navigationSelection = 0;
    }
    return DrawMenuButton("options_back", Localization::Text("VOLVER", "BACK"), ImVec2(centerX, panelMin.y + h * 0.78f), ImVec2(w * 0.42f, h * 0.09f), controllerNavigationActive && navigationSelection == 2)
        || ActivateSelection(2) || navBackPressed;
}

bool MainMenu::DrawLanguageSelector() {
    UpdateControllerNavigation(3);
    ImVec2 panelMin;
    ImVec2 panelMax;
    DrawTechPanel(Localization::Text("IDIOMA", "LANGUAGE"), panelMin, panelMax);
    const float w = panelMax.x - panelMin.x;
    const float h = panelMax.y - panelMin.y;
    const float centerX = (panelMin.x + panelMax.x) * 0.5f;
    const ImVec2 buttonSize(w * 0.62f, h * 0.105f);

    if (DrawMenuButton("language_spanish", "ESPAÑOL", ImVec2(centerX, panelMin.y + h * 0.34f), buttonSize, Localization::language == InterfaceLanguage::Spanish || (controllerNavigationActive && navigationSelection == 0)) || ActivateSelection(0)) {
        Localization::language = InterfaceLanguage::Spanish;
    }
    if (DrawMenuButton("language_english", "ENGLISH", ImVec2(centerX, panelMin.y + h * 0.52f), buttonSize, Localization::language == InterfaceLanguage::English || (controllerNavigationActive && navigationSelection == 1)) || ActivateSelection(1)) {
        Localization::language = InterfaceLanguage::English;
    }
    return DrawMenuButton("language_back", Localization::Text("VOLVER", "BACK"), ImVec2(centerX, panelMin.y + h * 0.78f), ImVec2(w * 0.42f, h * 0.09f), controllerNavigationActive && navigationSelection == 2)
        || ActivateSelection(2) || navBackPressed;
}

bool MainMenu::DrawImageHotspot(const char* id, const ImVec2& min, const ImVec2& max) {
    ImGui::SetCursorScreenPos(min);
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(max.x - min.x, max.y - min.y));
    if (pressed) {
        PlayButtonSound();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::GetWindowDrawList()->AddRect(min, max, IM_COL32(255, 215, 45, 180), 8.0f, 15, 2.0f);
    }
    return pressed;
}

void MainMenu::DrawContainedPanel(const Background& image, ImVec2& panelMin, ImVec2& panelMax) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    if (image.texture == 0 || image.width <= 0 || image.height <= 0) {
        panelMin = ImVec2(display.x * 0.25f, display.y * 0.05f);
        panelMax = ImVec2(display.x * 0.75f, display.y * 0.95f);
        return;
    }

    const float imageAspect = static_cast<float>(image.width) / static_cast<float>(image.height);
    const float maxWidth = display.x * 0.62f;
    const float maxHeight = display.y * 0.94f;
    float width = maxWidth;
    float height = width / imageAspect;
    if (height > maxHeight) {
        height = maxHeight;
        width = height * imageAspect;
    }
    panelMin = ImVec2((display.x - width) * 0.5f, (display.y - height) * 0.5f);
    panelMax = ImVec2(panelMin.x + width, panelMin.y + height);
    ImGui::GetWindowDrawList()->AddImage(
        reinterpret_cast<ImTextureID>(static_cast<intptr_t>(image.texture)),
        panelMin,
        panelMax,
        ImVec2(0.0f, 0.0f),
        ImVec2(1.0f, 1.0f));
}

bool MainMenu::DrawMenuButton(const char* id, const char* label, const ImVec2& center, const ImVec2& size, bool selected) {
    const ImVec2 min(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
    const ImVec2 max(center.x + size.x * 0.5f, center.y + size.y * 0.5f);
    ImGui::SetCursorScreenPos(min);
    const bool pressed = ImGui::InvisibleButton(id, size);
    if (pressed) {
        PlayButtonSound();
    }
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    const ImU32 fill = active ? IM_COL32(18, 83, 150, 255)
        : (hovered || selected) ? IM_COL32(24, 104, 184, 245)
        : IM_COL32(12, 35, 62, 245);
    const ImU32 border = (hovered || selected) ? IM_COL32(125, 205, 255, 255) : IM_COL32(75, 155, 225, 220);
    drawList->AddRectFilled(ImVec2(min.x + 5.0f, min.y + 6.0f), ImVec2(max.x + 5.0f, max.y + 6.0f), IM_COL32(0, 0, 0, 105), 5.0f);
    drawList->AddRectFilled(min, max, fill, 5.0f);
    drawList->AddRect(min, max, border, 5.0f, 15, (hovered || selected) ? 2.5f : 1.5f);
    drawList->AddLine(ImVec2(min.x + 12.0f, min.y + 8.0f), ImVec2(max.x - 12.0f, min.y + 8.0f), IM_COL32(180, 220, 255, 90), 1.0f);

    ImGui::PushFont(buttonFont);
    const ImVec2 textSize = ImGui::CalcTextSize(label);
    drawList->AddText(ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f), IM_COL32(240, 244, 250, 255), label);
    ImGui::PopFont();
    return pressed;
}

void MainMenu::DrawTechPanel(const char* title, ImVec2& panelMin, ImVec2& panelMax, float widthScale) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float width = (std::min)(display.x * widthScale, widthScale > 0.70f ? 1040.0f : 760.0f);
    const float height = (std::min)(display.y * 0.88f, 680.0f);
    panelMin = ImVec2((display.x - width) * 0.5f, (display.y - height) * 0.5f);
    panelMax = ImVec2(panelMin.x + width, panelMin.y + height);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(ImVec2(panelMin.x + 10.0f, panelMin.y + 12.0f), ImVec2(panelMax.x + 10.0f, panelMax.y + 12.0f), IM_COL32(0, 0, 0, 130), 8.0f);
    drawList->AddRectFilled(panelMin, panelMax, IM_COL32(10, 18, 29, 248), 8.0f);
    drawList->AddRect(panelMin, panelMax, IM_COL32(208, 158, 20, 255), 8.0f, 15, 3.0f);
    drawList->AddRect(ImVec2(panelMin.x + 10.0f, panelMin.y + 10.0f), ImVec2(panelMax.x - 10.0f, panelMax.y - 10.0f), IM_COL32(70, 135, 190, 130), 5.0f, 15, 1.5f);
    drawList->AddRectFilled(panelMin, ImVec2(panelMax.x, panelMin.y + height * 0.16f), IM_COL32(21, 42, 65, 255), 8.0f);
    drawList->AddLine(ImVec2(panelMin.x + 20.0f, panelMin.y + height * 0.16f), ImVec2(panelMax.x - 20.0f, panelMin.y + height * 0.16f), IM_COL32(242, 185, 0, 230), 2.0f);

    ImGui::PushFont(buttonFont);
    const ImVec2 titleSize = ImGui::CalcTextSize(title);
    drawList->AddText(ImVec2((display.x - titleSize.x) * 0.5f, panelMin.y + height * 0.065f - titleSize.y * 0.5f), IM_COL32(255, 202, 35, 255), title);
    ImGui::PopFont();
}

void MainMenu::UpdateControllerNavigation(int itemCount, bool horizontal) {
    navAcceptPressed = false;
    navBackPressed = false;
    const ImGuiIO& io = ImGui::GetIO();
    const bool mouseMoved = std::abs(io.MousePos.x - lastMouseX) > 0.75f || std::abs(io.MousePos.y - lastMouseY) > 0.75f;
    const bool mouseUsed = mouseMoved || ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1);
    lastMouseX = io.MousePos.x;
    lastMouseY = io.MousePos.y;
    if (mouseUsed) {
        controllerNavigationActive = false;
    }

    if (itemCount <= 0 || !glfwJoystickIsGamepad(GLFW_JOYSTICK_1)) {
        navigationSelection = (std::clamp)(navigationSelection, 0, (std::max)(itemCount - 1, 0));
        return;
    }

    GLFWgamepadstate state;
    if (!glfwGetGamepadState(GLFW_JOYSTICK_1, &state)) {
        return;
    }

    const bool upDown = state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] == GLFW_PRESS ||
        state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] < -0.55f;
    const bool downDown = state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] == GLFW_PRESS ||
        state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] > 0.55f;
    const bool leftDown = state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] == GLFW_PRESS ||
        state.axes[GLFW_GAMEPAD_AXIS_LEFT_X] < -0.55f;
    const bool rightDown = state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] == GLFW_PRESS ||
        state.axes[GLFW_GAMEPAD_AXIS_LEFT_X] > 0.55f;
    const bool acceptDown = state.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS;
    const bool backDown = state.buttons[GLFW_GAMEPAD_BUTTON_B] == GLFW_PRESS;
    const bool controllerUsed = upDown || downDown || leftDown || rightDown || acceptDown || backDown;
    if (controllerUsed) {
        controllerNavigationActive = true;
    }

    if (horizontal) {
        if (leftDown && !navLeftWasDown) {
            navigationSelection = (navigationSelection + itemCount - 1) % itemCount;
        }
        if (rightDown && !navRightWasDown) {
            navigationSelection = (navigationSelection + 1) % itemCount;
        }
        if (upDown && !navUpWasDown) {
            navigationSelection = (std::max)(navigationSelection - 1, 0);
        }
        if (downDown && !navDownWasDown) {
            navigationSelection = (std::min)(navigationSelection + 1, itemCount - 1);
        }
    }
    else {
        if (upDown && !navUpWasDown) {
            navigationSelection = (navigationSelection + itemCount - 1) % itemCount;
        }
        if (downDown && !navDownWasDown) {
            navigationSelection = (navigationSelection + 1) % itemCount;
        }
    }

    navAcceptPressed = acceptDown && !navAcceptWasDown;
    navBackPressed = backDown && !navBackWasDown;
    navUpWasDown = upDown;
    navDownWasDown = downDown;
    navLeftWasDown = leftDown;
    navRightWasDown = rightDown;
    navAcceptWasDown = acceptDown;
    navBackWasDown = backDown;
}

bool MainMenu::ActivateSelection(int index) {
    const bool selected = controllerNavigationActive && navAcceptPressed && navigationSelection == index;
    if (selected) {
        PlayButtonSound();
    }
    return selected;
}

void MainMenu::DrawKeyboardControls(const ImVec2& panelMin, const ImVec2& panelMax) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float w = panelMax.x - panelMin.x;
    const float h = panelMax.y - panelMin.y;
    const ImVec2 areaMin(panelMin.x + w * 0.10f, panelMin.y + h * 0.29f);
    const ImVec2 areaMax(panelMax.x - w * 0.10f, panelMin.y + h * 0.76f);
    drawList->AddRectFilled(areaMin, areaMax, IM_COL32(7, 13, 22, 225), 6.0f);
    drawList->AddRect(areaMin, areaMax, IM_COL32(65, 135, 200, 150), 6.0f, 15, 1.5f);

    auto drawKey = [&](const char* key, const char* action, float y) {
        const ImVec2 keyMin(areaMin.x + w * 0.08f, y);
        const ImVec2 keyMax(keyMin.x + w * 0.18f, y + h * 0.055f);
        drawList->AddRectFilled(ImVec2(keyMin.x + 3.0f, keyMin.y + 4.0f), ImVec2(keyMax.x + 3.0f, keyMax.y + 4.0f), IM_COL32(0, 0, 0, 120), 4.0f);
        drawList->AddRectFilled(keyMin, keyMax, IM_COL32(23, 44, 68, 255), 4.0f);
        drawList->AddRect(keyMin, keyMax, IM_COL32(242, 185, 0, 220), 4.0f, 15, 1.5f);
        ImGui::PushFont(bodyFont);
        const ImVec2 keySize = ImGui::CalcTextSize(key);
        drawList->AddText(ImVec2((keyMin.x + keyMax.x - keySize.x) * 0.5f, (keyMin.y + keyMax.y - keySize.y) * 0.5f), IM_COL32(255, 215, 72, 255), key);
        drawList->AddText(ImVec2(areaMin.x + w * 0.34f, y + h * 0.012f), IM_COL32(228, 234, 242, 255), action);
        ImGui::PopFont();
    };

    drawKey("W A S D", Localization::Text("MOVERSE", "MOVE"), areaMin.y + h * 0.035f);
    drawKey("MOUSE", Localization::Text("MOVER CAMARA", "MOVE CAMERA"), areaMin.y + h * 0.105f);
    drawKey("SHIFT", Localization::Text("CORRER", "RUN"), areaMin.y + h * 0.175f);
    drawKey("F3", Localization::Text("MENU DE AMBIENTE", "ENVIRONMENT MENU"), areaMin.y + h * 0.245f);
    drawKey("R", Localization::Text("ACTIVAR LLUVIA", "TOGGLE RAIN"), areaMin.y + h * 0.315f);
    drawKey("ESC", Localization::Text("MENU DE PAUSA", "PAUSE MENU"), areaMin.y + h * 0.385f);
}

void MainMenu::DrawGamepadControls(const ImVec2& panelMin, const ImVec2& panelMax) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float w = panelMax.x - panelMin.x;
    const float h = panelMax.y - panelMin.y;
    const ImVec2 areaMin(panelMin.x + w * 0.10f, panelMin.y + h * 0.29f);
    const ImVec2 areaMax(panelMax.x - w * 0.10f, panelMin.y + h * 0.76f);
    drawList->AddRectFilled(areaMin, areaMax, IM_COL32(7, 13, 22, 225), 6.0f);
    drawList->AddRect(areaMin, areaMax, IM_COL32(65, 135, 200, 150), 6.0f, 15, 1.5f);

    ImVec2 imageMin(areaMin.x + w * 0.26f, areaMin.y + h * 0.09f);
    float imageWidth = w * 0.48f;
    float imageHeight = imageWidth * 0.55f;
    if (dualsense.texture != 0) {
        imageHeight = imageWidth * static_cast<float>(dualsense.height) / (std::max)(static_cast<float>(dualsense.width), 1.0f);
        drawList->AddImage(
            reinterpret_cast<ImTextureID>(static_cast<intptr_t>(dualsense.texture)),
            imageMin,
            ImVec2(imageMin.x + imageWidth, imageMin.y + imageHeight),
            ImVec2(0.0f, 0.0f),
            ImVec2(1.0f, 1.0f));
    }

    auto drawCallout = [&](const char* button, const char* action, const ImVec2& labelPos, const ImVec2& target, bool leftSide) {
        ImGui::PushFont(bodyFont);
        const ImVec2 buttonSize = ImGui::CalcTextSize(button);
        const ImVec2 actionSize = ImGui::CalcTextSize(action);
        const float labelWidth = (std::max)(buttonSize.x, actionSize.x) + 18.0f;
        const float labelHeight = buttonSize.y + actionSize.y + 13.0f;
        const ImVec2 labelMin = leftSide ? labelPos : ImVec2(labelPos.x - labelWidth, labelPos.y);
        const ImVec2 labelMax(labelMin.x + labelWidth, labelMin.y + labelHeight);
        const ImVec2 anchor(leftSide ? labelMax.x : labelMin.x, labelMin.y + labelHeight * 0.5f);
        const ImVec2 elbow(leftSide ? anchor.x + 18.0f : anchor.x - 18.0f, anchor.y);

        drawList->AddRectFilled(labelMin, labelMax, IM_COL32(13, 29, 48, 245), 4.0f);
        drawList->AddRect(labelMin, labelMax, IM_COL32(79, 172, 235, 220), 4.0f, 15, 1.5f);
        drawList->AddText(ImVec2(labelMin.x + 9.0f, labelMin.y + 5.0f), IM_COL32(255, 202, 35, 255), button);
        drawList->AddText(ImVec2(labelMin.x + 9.0f, labelMin.y + 7.0f + buttonSize.y), IM_COL32(225, 233, 242, 255), action);
        drawList->AddLine(anchor, elbow, IM_COL32(105, 198, 255, 230), 1.8f);
        drawList->AddLine(elbow, target, IM_COL32(105, 198, 255, 230), 1.8f);
        drawList->AddCircleFilled(target, 3.5f, IM_COL32(255, 202, 35, 255));
        ImGui::PopFont();
    };

    const ImVec2 imageCenter(imageMin.x + imageWidth * 0.5f, imageMin.y + imageHeight * 0.5f);
    drawCallout(Localization::Text("STICK IZQ.", "LEFT STICK"), Localization::Text("MOVERSE", "MOVE"), ImVec2(areaMin.x + 10.0f, areaMin.y + h * 0.05f), ImVec2(imageMin.x + imageWidth * 0.30f, imageMin.y + imageHeight * 0.66f), true);
    drawCallout("L2", Localization::Text("CORRER", "RUN"), ImVec2(areaMin.x + 10.0f, areaMin.y + h * 0.28f), ImVec2(imageMin.x + imageWidth * 0.16f, imageMin.y + imageHeight * 0.18f), true);
    drawCallout(Localization::Text("CRUCETA", "D-PAD"), Localization::Text("NAVEGAR MENU", "NAVIGATE MENU"), ImVec2(areaMin.x + 10.0f, areaMin.y + h * 0.51f), ImVec2(imageMin.x + imageWidth * 0.27f, imageMin.y + imageHeight * 0.42f), true);
    drawCallout(Localization::Text("STICK DER.", "RIGHT STICK"), Localization::Text("MOVER CAMARA", "MOVE CAMERA"), ImVec2(areaMax.x - 10.0f, areaMin.y + h * 0.05f), ImVec2(imageMin.x + imageWidth * 0.68f, imageMin.y + imageHeight * 0.66f), false);
    drawCallout("R2", Localization::Text("AVANZAR", "DRIVE"), ImVec2(areaMax.x - 10.0f, areaMin.y + h * 0.28f), ImVec2(imageMin.x + imageWidth * 0.84f, imageMin.y + imageHeight * 0.18f), false);
    drawCallout(Localization::Text("X / CIRCULO", "X / CIRCLE"), Localization::Text("ACEPTAR / VOLVER", "ACCEPT / BACK"), ImVec2(areaMax.x - 10.0f, areaMin.y + h * 0.51f), ImVec2(imageMin.x + imageWidth * 0.79f, imageMin.y + imageHeight * 0.42f), false);

    ImGui::PushFont(bodyFont);
    const char* footer = Localization::Text("CUADRADO: INTERACTUAR     OPTIONS: PAUSA", "SQUARE: INTERACT     OPTIONS: PAUSE");
    const ImVec2 footerSize = ImGui::CalcTextSize(footer);
    drawList->AddText(ImVec2(imageCenter.x - footerSize.x * 0.5f, areaMax.y - h * 0.075f), IM_COL32(225, 233, 242, 255), footer);
    ImGui::PopFont();
}
