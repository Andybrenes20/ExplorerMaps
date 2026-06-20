#pragma once

#include <functional>
#include <vector>

struct ImFont;
struct ImVec2;
class EnvironmentSystem;

enum class MainMenuAction {
    None,
    Start,
    Resume,
    TravelCity,
    TravelClock,
    TravelStatue,
    TravelVolcano,
    ReturnToMainMenu,
    Quit
};

class MainMenu {
public:
    struct Background {
        unsigned int texture = 0;
        int width = 0;
        int height = 0;
    };

    bool Initialize();
    void SetButtonSoundCallback(std::function<void()> callback);
    MainMenuAction Draw(float currentTime);
    MainMenuAction DrawPause(EnvironmentSystem& environment);
    void DrawEnvironmentMenu(EnvironmentSystem& environment);
    void Shutdown();

private:
    std::vector<Background> backgrounds;
    Background logo;
    Background pauseMenu;
    Background atmosphereMenu;
    Background dualsense;
    ImFont* titleFont = nullptr;
    ImFont* buttonFont = nullptr;
    ImFont* bodyFont = nullptr;
    bool creditsOpen = false;
    bool optionsOpen = false;
    enum class PausePage { Main, Places, Atmosphere, Controls, Options };
    PausePage pausePage = PausePage::Main;
    enum class OptionsPage { Main, Language };
    OptionsPage optionsPage = OptionsPage::Main;
    enum class ControlsPage { Keyboard, Gamepad };
    ControlsPage controlsPage = ControlsPage::Keyboard;
    int navigationSelection = 0;
    bool navUpWasDown = false;
    bool navDownWasDown = false;
    bool navLeftWasDown = false;
    bool navRightWasDown = false;
    bool navAcceptWasDown = false;
    bool navBackWasDown = false;
    bool navAcceptPressed = false;
    bool navBackPressed = false;
    bool controllerNavigationActive = false;
    float lastMouseX = 0.0f;
    float lastMouseY = 0.0f;
    std::function<void()> buttonSoundCallback;

    void PlayButtonSound();
    bool LoadBackground(const char* path);
    bool LoadTexture(const char* path, Background& texture);
    bool DrawAngledButton(const char* id, const char* label, const ImVec2& center, const ImVec2& size, bool selected = false);
    void DrawBackground(float currentTime);
    void DrawCredits();
    bool DrawOptions();
    bool DrawLanguageSelector();
    bool DrawImageHotspot(const char* id, const ImVec2& min, const ImVec2& max);
    void DrawContainedPanel(const Background& image, ImVec2& panelMin, ImVec2& panelMax);
    bool DrawMenuButton(const char* id, const char* label, const ImVec2& center, const ImVec2& size, bool selected = false);
    void DrawTechPanel(const char* title, ImVec2& panelMin, ImVec2& panelMax, float widthScale = 0.50f);
    void UpdateControllerNavigation(int itemCount, bool horizontal = false);
    bool ActivateSelection(int index);
    void DrawKeyboardControls(const ImVec2& panelMin, const ImVec2& panelMax);
    void DrawGamepadControls(const ImVec2& panelMin, const ImVec2& panelMax);
};
