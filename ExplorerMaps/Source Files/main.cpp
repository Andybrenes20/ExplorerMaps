#define _CRT_SECURE_NO_WARNINGS

//------- Ignore this ----------
#include<algorithm>
#include<array>
#include<cmath>
#include<filesystem>
#include<iomanip>
#include<sstream>
#include<string>
#include<vector>
#include<stb/stb_easy_font.h>
namespace fs = std::filesystem;
//------------------------------

#ifdef _WIN32
extern "C"
{
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 0x00000001;
}
#endif

#include "Model.h"
#include "Skybox.h"
#include "Editor.h"
#include "CollisionEditor.h"

#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// --- Ventana / escena ----------------------------------
const unsigned int width = 1920;
const unsigned int height = 1080;
const float targetSceneRadius = 1800.0f;
const float cameraFov = 55.0f;
const float cameraNearPlane = 0.05f;
const float cameraFarPlane = 6000.0f;
const float celestialOrbitRadius = 3200.0f;
const float maxSunHeight = 3000.0f;
const float maxMoonHeightFactor = 0.82f;

// --- Caminar -------------------------------------------
const float walkEyeHeight = 6.0f;
const float walkProbeRadius = 8.0f;
const float walkMaxStepUp = 12.0f;
const float walkMaxDropDown = 45.0f;
const float walkMaxSlopeDegrees = 68.0f;
const float walkSpeed = 30.0f;
const float cameraCollisionRadius = 6.0f;

// --- Opciones ----------------w--------------------------
const bool showCoordinatesInWindowTitle = true;
const bool useFastRenderMode = false;
const glm::vec3 blockedZoneMin = glm::vec3(240.0f, -260.0f, 360.0f);
const glm::vec3 blockedZoneMax = glm::vec3(310.0f, -150.0f, 435.0f);

// CICLO MUY RAPIDO PARA VIDEO
const float dayNightSpeed = 0.01f;
const bool useProceduralDaySkybox = true;

const float SUN_SIZE = 50.0f;
const float MOON_SIZE = 30.0f;
const int lampGlowSectors = 16;
const int lampGlowIndexCount = lampGlowSectors * (lampGlowSectors - 1) * 6;
const float lampGlowSize = 1.0f;
const float lampCoreSize = 0.20f;
const float lampHaloSize = 20.0f;
constexpr std::size_t maxLampLightCount = 12;
const std::array<glm::vec3, 2> defaultLampLightPositions =
{
    glm::vec3(130.0f, -159.0f, 572.0f),
    glm::vec3(-19.022f, -159.0f, 572.0f)
};
const glm::vec3 defaultLampLightColor = glm::vec3(1.0f, 0.92f, 0.78f);
const float defaultLampLightRadius = 50.0f;
const float defaultLampLightIntensity = 2.10f;

enum class EnvironmentMode
{
    Auto,
    Day,
    Night,
    Manual
};

struct EnvironmentMenuState
{
    bool open = false;
    int selection = 0;
    bool menuToggleWasDown = false;
    bool upWasDown = false;
    bool downWasDown = false;
    bool enterWasDown = false;
    bool escapeWasDown = false;
    bool gamepadOptionsWasDown = false;
    bool gamepadUpWasDown = false;
    bool gamepadDownWasDown = false;
    bool gamepadAcceptWasDown = false;
    bool gamepadCancelWasDown = false;
    bool mouseLeftWasDown = false;
};

struct SkyPanelState
{
    bool open = false;
    int selection = 0;
    bool toggleWasDown = false;
    bool upWasDown = false;
    bool downWasDown = false;
    bool leftWasDown = false;
    bool rightWasDown = false;
};

constexpr int kEnvironmentMenuItemCount = 4;
constexpr int kEnvironmentMenuExitIndex = 3;
constexpr float kManualTimeStep = 1.0f / 24.0f;
constexpr int kSkyPanelItemCount = 3;

bool IsInsideRect(double px, double py, float x, float y, float w, float h)
{
    return px >= x && px <= (x + w) && py >= y && py <= (y + h);
}

int GetEnvironmentMenuRowAt(double mouseX, double mouseY)
{
    const float panelX = 580.0f;
    const float panelY = 245.0f;
    const float rowX = panelX + 76.0f;
    const float rowW = 608.0f;

    for (int i = 0; i < kEnvironmentMenuItemCount; ++i)
    {
        const float rowY = panelY + 172.0f + i * 78.0f;
        if (IsInsideRect(mouseX, mouseY, rowX, rowY, rowW, 58.0f))
            return i;
    }

    return -1;
}

float ComputeSkyBlendFactor(float sunHeight)
{
    return glm::smoothstep(0.28f, -0.18f, sunHeight);
}

const char* EnvironmentModeName(EnvironmentMode mode)
{
    switch (mode)
    {
    case EnvironmentMode::Day: return "DIA";
    case EnvironmentMode::Night: return "NOCHE";
    case EnvironmentMode::Manual: return "MANUAL";
    default: return "AUTO";
    }
}

EnvironmentMode EnvironmentModeFromSelection(int selection)
{
    switch (selection)
    {
    case 0: return EnvironmentMode::Day;
    case 1: return EnvironmentMode::Night;
    case 2: return EnvironmentMode::Auto;
    default: return EnvironmentMode::Auto;
    }
}

int EnvironmentSelectionFromMode(EnvironmentMode mode)
{
    switch (mode)
    {
    case EnvironmentMode::Day: return 0;
    case EnvironmentMode::Night: return 1;
    case EnvironmentMode::Auto: return 2;
    default: return 2;
    }
}

float WrapTimeOfDay(float timeOfDay)
{
    const float wrapped = std::fmod(timeOfDay, 1.0f);
    return wrapped < 0.0f ? wrapped + 1.0f : wrapped;
}

float TimeOfDayToSunAngle(float timeOfDay)
{
    return WrapTimeOfDay(timeOfDay) * 6.2831853f;
}

float TimeOfDayToClockHours(float timeOfDay)
{
    return WrapTimeOfDay(timeOfDay) * 24.0f + 6.0f;
}

std::string FormatTimeOfDay(float timeOfDay)
{
    float clockHours = std::fmod(TimeOfDayToClockHours(timeOfDay), 24.0f);
    if (clockHours < 0.0f)
        clockHours += 24.0f;

    const int hours = static_cast<int>(clockHours);
    const int minutes = static_cast<int>((clockHours - static_cast<float>(hours)) * 60.0f + 0.5f) % 60;

    std::ostringstream stream;
    stream << std::setfill('0') << std::setw(2) << hours << ":" << std::setw(2) << minutes;
    return stream.str();
}

EnvironmentMode ResolveSkyboxMode(EnvironmentMode environmentMode, bool isDay)
{
    if (environmentMode == EnvironmentMode::Day)
        return EnvironmentMode::Day;

    if (environmentMode == EnvironmentMode::Night)
        return EnvironmentMode::Night;

    return isDay ? EnvironmentMode::Day : EnvironmentMode::Night;
}

bool HandleEnvironmentMenu(GLFWwindow* window, EnvironmentMenuState& menu, EnvironmentMode& mode, bool& shouldExit)
{
    const bool menuToggleDown = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    const bool upDown = glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    const bool downDown = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    const bool enterDown = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
    const bool escapeDown = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    const bool mouseLeftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool gamepadOptionsDown = false;
    bool gamepadUpDown = false;
    bool gamepadDownDown = false;
    bool gamepadAcceptDown = false;
    bool gamepadCancelDown = false;
    bool modeChanged = false;

    if (glfwJoystickIsGamepad(GLFW_JOYSTICK_1))
    {
        GLFWgamepadstate state;
        if (glfwGetGamepadState(GLFW_JOYSTICK_1, &state))
        {
            const float stickDeadzone = 0.55f;
            gamepadOptionsDown = state.buttons[GLFW_GAMEPAD_BUTTON_START] == GLFW_PRESS;
            gamepadUpDown = state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] == GLFW_PRESS ||
                state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] < -stickDeadzone;
            gamepadDownDown = state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] == GLFW_PRESS ||
                state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] > stickDeadzone;
            gamepadAcceptDown = state.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS;
            gamepadCancelDown = state.buttons[GLFW_GAMEPAD_BUTTON_B] == GLFW_PRESS;
        }
    }

    const bool keyboardTogglePressed = menuToggleDown && !menu.menuToggleWasDown;
    const bool gamepadTogglePressed = gamepadOptionsDown && !menu.gamepadOptionsWasDown;

    if (keyboardTogglePressed || gamepadTogglePressed)
    {
        menu.open = !menu.open;
        menu.selection = EnvironmentSelectionFromMode(mode);
    }

    if (menu.open)
    {
        if ((upDown && !menu.upWasDown) || (gamepadUpDown && !menu.gamepadUpWasDown))
            menu.selection = (menu.selection + kEnvironmentMenuItemCount - 1) % kEnvironmentMenuItemCount;

        if ((downDown && !menu.downWasDown) || (gamepadDownDown && !menu.gamepadDownWasDown))
            menu.selection = (menu.selection + 1) % kEnvironmentMenuItemCount;

        double mouseX = 0.0;
        double mouseY = 0.0;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        const int hoveredRow = GetEnvironmentMenuRowAt(mouseX, mouseY);
        if (hoveredRow >= 0)
            menu.selection = hoveredRow;

        if ((enterDown && !menu.enterWasDown) || (gamepadAcceptDown && !menu.gamepadAcceptWasDown))
        {
            if (menu.selection == kEnvironmentMenuExitIndex)
            {
                shouldExit = true;
                menu.open = false;
            }
            else
            {
                mode = EnvironmentModeFromSelection(menu.selection);
                menu.open = false;
                modeChanged = true;
            }
        }

        if (hoveredRow >= 0 && mouseLeftDown && !menu.mouseLeftWasDown)
        {
            if (hoveredRow == kEnvironmentMenuExitIndex)
            {
                shouldExit = true;
                menu.open = false;
            }
            else
            {
                menu.selection = hoveredRow;
                mode = EnvironmentModeFromSelection(hoveredRow);
                menu.open = false;
                modeChanged = true;
            }
        }

        if ((gamepadCancelDown && !menu.gamepadCancelWasDown) ||
            ((escapeDown && !menu.escapeWasDown) && !keyboardTogglePressed))
            menu.open = false;
    }

    menu.menuToggleWasDown = menuToggleDown;
    menu.upWasDown = upDown;
    menu.downWasDown = downDown;
    menu.enterWasDown = enterDown;
    menu.escapeWasDown = escapeDown;
    menu.gamepadOptionsWasDown = gamepadOptionsDown;
    menu.gamepadUpWasDown = gamepadUpDown;
    menu.gamepadDownWasDown = gamepadDownDown;
    menu.gamepadAcceptWasDown = gamepadAcceptDown;
    menu.gamepadCancelWasDown = gamepadCancelDown;
    menu.mouseLeftWasDown = mouseLeftDown;

    return modeChanged;
}

bool HandleSkyPanel(GLFWwindow* window, SkyPanelState& panel, EnvironmentMode& mode, float& manualTimeOfDay)
{
    const bool toggleDown = glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS;
    if (toggleDown && !panel.toggleWasDown)
        panel.open = !panel.open;

    panel.toggleWasDown = toggleDown;
    return panel.open;
}

void ApplyEnvironmentMode(EnvironmentMode mode, float manualTimeOfDay, float& sunAngleRad, float& sunHeight)
{
    if (mode == EnvironmentMode::Day)
    {
        sunAngleRad = 1.5708f;
        sunHeight = 1.0f;
    }
    else if (mode == EnvironmentMode::Night)
    {
        sunAngleRad = 4.71239f;
        sunHeight = -1.0f;
    }
    else if (mode == EnvironmentMode::Manual)
    {
        sunAngleRad = TimeOfDayToSunAngle(manualTimeOfDay);
        sunHeight = std::sin(sunAngleRad);
    }
}

std::string BuildEnvironmentTitle(const EnvironmentMenuState& menu, EnvironmentMode mode, float manualTimeOfDay, const glm::vec3& normPos)
{
    std::ostringstream title;
    title << std::fixed << std::setprecision(3);

    if (menu.open)
    {
        const char* option0 = menu.selection == 0 ? "> DIA" : "  DIA";
        const char* option1 = menu.selection == 1 ? "> NOCHE" : "  NOCHE";
        const char* option2 = menu.selection == 2 ? "> AUTO" : "  AUTO";
        const char* option3 = menu.selection == 3 ? "> SALIR" : "  SALIR";
        title << "MENU AMBIENTE | " << option0 << " | " << option1 << " | " << option2 << " | " << option3
              << " | Enter: aplicar | Esc: cerrar";
    }
    else
    {
        title << "Ambiente: " << EnvironmentModeName(mode)
              << " | Hora: " << FormatTimeOfDay(manualTimeOfDay)
              << " | Esc: menu | F3: panel cielo | X:" << normPos.x << " Y:" << normPos.y << " Z:" << normPos.z;
    }

    return title.str();
}

struct OverlayVertex
{
    float x;
    float y;
    float r;
    float g;
    float b;
    float a;
};

void AddOverlayRect(std::vector<OverlayVertex>& vertices, float x, float y, float w, float h, const glm::vec4& color)
{
    const float x0 = (x / width) * 2.0f - 1.0f;
    const float y0 = 1.0f - (y / height) * 2.0f;
    const float x1 = ((x + w) / width) * 2.0f - 1.0f;
    const float y1 = 1.0f - ((y + h) / height) * 2.0f;

    vertices.push_back({ x0, y0, color.r, color.g, color.b, color.a });
    vertices.push_back({ x1, y0, color.r, color.g, color.b, color.a });
    vertices.push_back({ x1, y1, color.r, color.g, color.b, color.a });
    vertices.push_back({ x0, y0, color.r, color.g, color.b, color.a });
    vertices.push_back({ x1, y1, color.r, color.g, color.b, color.a });
    vertices.push_back({ x0, y1, color.r, color.g, color.b, color.a });
}

void AddOverlayText(std::vector<OverlayVertex>& vertices, const std::string& text, float x, float y, float scale, const glm::vec4& color)
{
    if (text.empty())
        return;

    struct EasyFontVertex
    {
        float x;
        float y;
        float z;
        unsigned char color[4];
    };

    const float fontScale = scale * 2.8f;
    std::vector<char> printableText(text.begin(), text.end());
    printableText.push_back('\0');

    std::vector<char> textBuffer((text.size() + 1) * 512, 0);
    const int quadCount = stb_easy_font_print(0.0f, 0.0f, printableText.data(), nullptr, textBuffer.data(), static_cast<int>(textBuffer.size()));
    const auto* glyphVertices = reinterpret_cast<const EasyFontVertex*>(textBuffer.data());

    for (int i = 0; i < quadCount; ++i)
    {
        const EasyFontVertex& v0 = glyphVertices[i * 4 + 0];
        const EasyFontVertex& v1 = glyphVertices[i * 4 + 1];
        const EasyFontVertex& v2 = glyphVertices[i * 4 + 2];

        AddOverlayRect(
            vertices,
            x + v0.x * fontScale,
            y + v0.y * fontScale,
            (v1.x - v0.x) * fontScale,
            (v2.y - v0.y) * fontScale,
            color
        );
    }
}

void DrawEnvironmentMenu(const EnvironmentMenuState& menu, EnvironmentMode mode, float manualTimeOfDay, Shader& overlayShader, GLuint overlayVAO, GLuint overlayVBO)
{
    std::vector<OverlayVertex> vertices;

    const glm::vec4 panelColor(0.015f, 0.018f, 0.026f, 0.88f);
    const glm::vec4 panelEdge(0.95f, 0.76f, 0.28f, 0.95f);
    const glm::vec4 rowColor(0.055f, 0.065f, 0.085f, 0.88f);
    const glm::vec4 rowSelected(0.16f, 0.28f, 0.40f, 0.96f);
    const glm::vec4 textMain(0.92f, 0.90f, 0.82f, 1.0f);
    const glm::vec4 textMuted(0.52f, 0.60f, 0.70f, 1.0f);
    const glm::vec4 accent(0.98f, 0.78f, 0.24f, 1.0f);

    if (menu.open)
    {
        const float panelX = 580.0f;
        const float panelY = 245.0f;
        const float panelW = 760.0f;
        const float panelH = 560.0f;

        AddOverlayRect(vertices, panelX + 10.0f, panelY + 12.0f, panelW, panelH, glm::vec4(0.0f, 0.0f, 0.0f, 0.28f));
        AddOverlayRect(vertices, panelX, panelY, panelW, panelH, panelColor);
        AddOverlayRect(vertices, panelX, panelY, panelW, 5.0f, panelEdge);
        AddOverlayRect(vertices, panelX, panelY + panelH - 5.0f, panelW, 5.0f, glm::vec4(0.08f, 0.10f, 0.14f, 0.90f));

        AddOverlayText(vertices, "AMBIENTE", panelX + 76.0f, panelY + 58.0f, 1.7f, textMain);
        AddOverlayText(vertices, "SELECCIONA SKYBOX", panelX + 76.0f, panelY + 112.0f, 0.92f, textMuted);

        const char* labels[kEnvironmentMenuItemCount] = { "DIA", "NOCHE", "AUTO", "SALIR" };
        for (int i = 0; i < kEnvironmentMenuItemCount; ++i)
        {
            const float rowX = panelX + 76.0f;
            const float rowY = panelY + 172.0f + i * 78.0f;
            const bool selected = menu.selection == i;
            const bool isExit = i == kEnvironmentMenuExitIndex;
            const bool active = !isExit && EnvironmentSelectionFromMode(mode) == i;

            const glm::vec4 currentRowColor = isExit
                ? (selected ? glm::vec4(0.32f, 0.18f, 0.18f, 0.96f) : glm::vec4(0.14f, 0.07f, 0.07f, 0.88f))
                : (selected ? rowSelected : rowColor);
            const glm::vec4 currentStripeColor = isExit
                ? (selected ? glm::vec4(1.0f, 0.48f, 0.38f, 1.0f) : glm::vec4(0.34f, 0.16f, 0.16f, 1.0f))
                : (selected ? accent : glm::vec4(0.18f, 0.22f, 0.28f, 1.0f));

            AddOverlayRect(vertices, rowX, rowY, 608.0f, 58.0f, currentRowColor);
            AddOverlayRect(vertices, rowX, rowY, 5.0f, 58.0f, currentStripeColor);

            if (selected)
                AddOverlayText(vertices, ">", rowX + 28.0f, rowY + 17.0f, 1.4f, isExit ? glm::vec4(1.0f, 0.48f, 0.38f, 1.0f) : accent);

            AddOverlayText(vertices, labels[i], rowX + 86.0f, rowY + 17.0f, 1.35f, isExit
                ? (selected ? glm::vec4(1.0f, 0.90f, 0.86f, 1.0f) : glm::vec4(0.88f, 0.70f, 0.70f, 1.0f))
                : (selected ? glm::vec4(1.0f, 0.96f, 0.82f, 1.0f) : glm::vec4(0.74f, 0.80f, 0.88f, 1.0f)));

            if (active)
                AddOverlayText(vertices, "ACTIVO", rowX + 410.0f, rowY + 21.0f, 0.80f, selected ? accent : textMuted);
        }

        AddOverlayRect(vertices, panelX + 76.0f, panelY + 500.0f, 608.0f, 1.5f, glm::vec4(0.22f, 0.25f, 0.31f, 0.9f));
        AddOverlayText(vertices, "W/S PAD MOVER", panelX + 92.0f, panelY + 518.0f, 0.58f, textMuted);
        AddOverlayText(vertices, "CLICK APLICAR", panelX + 300.0f, panelY + 518.0f, 0.56f, textMuted);
        AddOverlayText(vertices, "OPTIONS/B CERRAR", panelX + 500.0f, panelY + 518.0f, 0.52f, textMuted);
    }
    else
    {
        AddOverlayRect(vertices, 32.0f, 28.0f, 460.0f, 46.0f, glm::vec4(0.015f, 0.018f, 0.026f, 0.70f));
        AddOverlayRect(vertices, 32.0f, 28.0f, 4.0f, 46.0f, accent);
        AddOverlayText(vertices, std::string("AMBIENTE: ") + EnvironmentModeName(mode), 52.0f, 43.0f, 0.86f, textMain);
        AddOverlayText(vertices, "ESC", 412.0f, 43.0f, 0.72f, accent);
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    overlayShader.Activate();
    glBindVertexArray(overlayVAO);
    glBindBuffer(GL_ARRAY_BUFFER, overlayVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(OverlayVertex), vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void DrawSkyControlPanel(const SkyPanelState& panel, EnvironmentMode mode, float manualTimeOfDay, Shader& overlayShader, GLuint overlayVAO, GLuint overlayVBO)
{
    if (!panel.open)
        return;

    std::vector<OverlayVertex> vertices;
    const glm::vec4 panelColor(0.07f, 0.09f, 0.13f, 0.86f);
    const glm::vec4 topBar(0.11f, 0.78f, 0.84f, 0.96f);
    const glm::vec4 textMain(0.92f, 0.94f, 0.98f, 1.0f);
    const glm::vec4 textMuted(0.67f, 0.73f, 0.83f, 1.0f);
    const glm::vec4 accent(1.0f, 0.82f, 0.22f, 1.0f);
    const glm::vec4 rowColor(0.09f, 0.11f, 0.16f, 0.92f);
    const glm::vec4 selectedColor(0.16f, 0.24f, 0.38f, 0.96f);

    const float panelX = 48.0f;
    const float panelY = 64.0f;
    const float panelW = 300.0f;
    const float panelH = 248.0f;

    AddOverlayRect(vertices, panelX, panelY, panelW, panelH, panelColor);
    AddOverlayRect(vertices, panelX, panelY, panelW, 10.0f, topBar);
    AddOverlayText(vertices, "TerrainEngine OpenGL style", panelX + 14.0f, panelY + 27.0f, 0.38f, textMuted);
    AddOverlayText(vertices, "Sky controls", panelX + 14.0f, panelY + 48.0f, 0.50f, accent);

    const float rowX = panelX + 14.0f;
    const float rowW = panelW - 28.0f;

    AddOverlayRect(vertices, rowX, panelY + 68.0f, rowW, 24.0f, panel.selection == 0 ? selectedColor : rowColor);
    AddOverlayText(vertices, "Mode", rowX + 10.0f, panelY + 83.0f, 0.42f, textMain);
    AddOverlayText(vertices, EnvironmentModeName(mode), rowX + 180.0f, panelY + 83.0f, 0.42f, panel.selection == 0 ? accent : textMuted);

    AddOverlayRect(vertices, rowX, panelY + 100.0f, rowW, 24.0f, panel.selection == 1 ? selectedColor : rowColor);
    AddOverlayText(vertices, "Hour", rowX + 10.0f, panelY + 115.0f, 0.42f, textMain);
    AddOverlayText(vertices, FormatTimeOfDay(manualTimeOfDay), rowX + 180.0f, panelY + 115.0f, 0.42f, panel.selection == 1 ? accent : textMuted);

    AddOverlayRect(vertices, rowX, panelY + 138.0f, rowW, 54.0f, panel.selection == 2 ? selectedColor : rowColor);
    AddOverlayText(vertices, "Sky model", rowX + 10.0f, panelY + 153.0f, 0.42f, textMain);
    AddOverlayText(vertices, "Procedural repo-like", rowX + 112.0f, panelY + 153.0f, 0.38f, accent);
    AddOverlayText(vertices, "No cubemap textures", rowX + 10.0f, panelY + 173.0f, 0.34f, textMuted);
    AddOverlayText(vertices, "Day/night from shader", rowX + 128.0f, panelY + 173.0f, 0.34f, textMuted);

    AddOverlayText(vertices, "F3 toggle", panelX + 14.0f, panelY + 220.0f, 0.34f, textMuted);
    AddOverlayText(vertices, "W/S select", panelX + 84.0f, panelY + 220.0f, 0.34f, textMuted);
    AddOverlayText(vertices, "A/D edit", panelX + 168.0f, panelY + 220.0f, 0.34f, textMuted);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    overlayShader.Activate();
    glBindVertexArray(overlayVAO);
    glBindBuffer(GL_ARRAY_BUFFER, overlayVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(OverlayVertex), vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void SetupSkyPanelStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(10.0f, 8.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.08f, 0.12f, 0.90f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.06f, 0.08f, 0.12f, 0.96f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.65f, 0.78f, 0.96f);
    colors[ImGuiCol_Header] = ImVec4(0.13f, 0.17f, 0.24f, 0.92f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.24f, 0.34f, 0.98f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.22f, 0.30f, 0.44f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.13f, 0.17f, 0.24f, 0.92f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.26f, 0.38f, 0.98f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.22f, 0.32f, 0.48f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.12f, 0.18f, 0.95f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.14f, 0.18f, 0.28f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.24f, 0.34f, 1.0f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.95f, 0.77f, 0.24f, 0.96f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.86f, 0.34f, 1.0f);
    colors[ImGuiCol_Text] = ImVec4(0.93f, 0.94f, 0.98f, 1.0f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.58f, 0.64f, 0.74f, 1.0f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.95f, 0.77f, 0.24f, 1.0f);
}

void DrawSkyControlImGui(SkyPanelState& panel, EnvironmentMode& mode, float& manualTimeOfDay, SkyCloudSettings& cloudSettings)
{
    if (!panel.open)
        return;

    ImGui::SetNextWindowPos(ImVec2(48.0f, 64.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(310.0f, 520.0f), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

    bool open = panel.open;
    if (!ImGui::Begin("Scene controls", &open, flags))
    {
        ImGui::End();
        panel.open = open;
        return;
    }
    panel.open = open;

    ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.22f, 1.0f), "Clouds controls");

    static const char* modeNames[] = { "AUTO", "DIA", "NOCHE", "MANUAL" };
    int modeIndex = mode == EnvironmentMode::Auto ? 0 : mode == EnvironmentMode::Day ? 1 : mode == EnvironmentMode::Night ? 2 : 3;
    if (ImGui::SliderInt("Mode", &modeIndex, 0, 3, modeNames[modeIndex]))
        mode = modeIndex == 0 ? EnvironmentMode::Auto : modeIndex == 1 ? EnvironmentMode::Day : modeIndex == 2 ? EnvironmentMode::Night : EnvironmentMode::Manual;

    float hour = WrapTimeOfDay(manualTimeOfDay);
    if (ImGui::SliderFloat("Hour", &hour, 0.0f, 1.0f, FormatTimeOfDay(hour).c_str()))
    {
        manualTimeOfDay = hour;
        mode = EnvironmentMode::Manual;
    }

    ImGui::SliderFloat("Coverage", &cloudSettings.coverage, 0.20f, 1.20f);
    ImGui::SliderFloat("Speed", &cloudSettings.speed, 0.10f, 3.50f);
    ImGui::SliderFloat("Crispiness", &cloudSettings.crispiness, 0.35f, 2.50f);
    ImGui::SliderFloat("Curliness", &cloudSettings.curliness, 0.10f, 2.50f);
    ImGui::SliderFloat("Density", &cloudSettings.density, 0.20f, 1.60f);

    if (ImGui::Button("Default clouds"))
    {
        cloudSettings.coverage = 0.86f;
        cloudSettings.speed = 1.05f;
        cloudSettings.crispiness = 0.92f;
        cloudSettings.curliness = 0.92f;
        cloudSettings.density = 1.18f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Heavy clouds"))
    {
        cloudSettings.coverage = 1.08f;
        cloudSettings.speed = 1.20f;
        cloudSettings.crispiness = 0.82f;
        cloudSettings.curliness = 1.18f;
        cloudSettings.density = 1.48f;
    }

    ImGui::Text("Current time: %s", FormatTimeOfDay(manualTimeOfDay).c_str());
    ImGui::Text("Sky model: procedural repo-like");
    ImGui::Separator();

    ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.22f, 1.0f), "Sky controls");
    ImGui::Text("No cubemap textures");
    ImGui::Text("Day/night generated in shader");
    ImGui::Text("Mouse enabled while this panel is open");
    ImGui::Separator();

    ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.22f, 1.0f), "Shortcuts");
    ImGui::BulletText("F3 abre/cierra este panel");
    ImGui::BulletText("ESC abre el menu clasico");
    ImGui::BulletText("J/L mueve el tiempo rapido");
    ImGui::BulletText("Mouse controla sliders y botones");
    ImGui::Separator();

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
}
std::vector<std::string> GetSkyboxFacePaths(EnvironmentMode mode)
{
    return {};
}
void createSphere(GLuint& VAO, GLuint& VBO, GLuint& EBO, int sectors, float radius) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    float x, y, z, xy;
    float nx, ny, nz;
    float sectorStep = 2 * 3.14159f / sectors;
    float stackStep = 3.14159f / sectors;
    float sectorAngle, stackAngle;

    for (int i = 0; i <= sectors; ++i) {
        stackAngle = 3.14159f / 2 - i * stackStep;
        xy = radius * cosf(stackAngle);
        z = radius * sinf(stackAngle);

        for (int j = 0; j <= sectors; ++j) {
            sectorAngle = j * sectorStep;
            x = xy * cosf(sectorAngle);
            y = xy * sinf(sectorAngle);

            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            nx = x / radius;
            ny = y / radius;
            nz = z / radius;
            vertices.push_back(nx);
            vertices.push_back(ny);
            vertices.push_back(nz);
        }
    }

    for (int i = 0; i < sectors; ++i) {
        int k1 = i * (sectors + 1);
        int k2 = k1 + sectors + 1;

        for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }
            if (i != (sectors - 1)) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void createCube(GLuint& VAO, GLuint& VBO)
{
    const float vertices[] =
    {
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f, 0.0f,

         0.5f,  0.5f,  0.5f, 1.0f,  0.0f, 0.0f,
         0.5f,  0.5f, -0.5f, 1.0f,  0.0f, 0.0f,
         0.5f, -0.5f, -0.5f, 1.0f,  0.0f, 0.0f,
         0.5f, -0.5f, -0.5f, 1.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f, 1.0f,  0.0f, 0.0f,
         0.5f,  0.5f,  0.5f, 1.0f,  0.0f, 0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f, 0.0f,

        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

glm::mat4 BuildSceneModelTransform(const Entity& entity, const glm::mat4& baseModelTransform)
{
    return ComposeEntityMatrix(entity) * baseModelTransform;
}

void UploadLampLightUniforms(Shader& shaderProgram, const std::vector<Light>& lights)
{
    std::array<glm::vec3, maxLampLightCount> lightPositions{};
    std::array<glm::vec3, maxLampLightCount> lightColors{};
    std::array<float, maxLampLightCount> lightRadii{};
    std::array<float, maxLampLightCount> lightIntensities{};

    const int activeLightCount = static_cast<int>(std::min<std::size_t>(lights.size(), maxLampLightCount));
    for (int i = 0; i < activeLightCount; ++i)
    {
        lightPositions[i] = lights[i].position;
        lightColors[i] = lights[i].color;
        lightRadii[i] = lights[i].radius;
        lightIntensities[i] = lights[i].intensity;
    }

    glUniform1i(glGetUniformLocation(shaderProgram.ID, "lampLightCount"), activeLightCount);
    glUniform3fv(glGetUniformLocation(shaderProgram.ID, "lampLightPositions[0]"), static_cast<GLsizei>(maxLampLightCount), glm::value_ptr(lightPositions[0]));
    glUniform3fv(glGetUniformLocation(shaderProgram.ID, "lampLightColors[0]"), static_cast<GLsizei>(maxLampLightCount), glm::value_ptr(lightColors[0]));
    glUniform1fv(glGetUniformLocation(shaderProgram.ID, "lampLightRadii[0]"), static_cast<GLsizei>(maxLampLightCount), lightRadii.data());
    glUniform1fv(glGetUniformLocation(shaderProgram.ID, "lampLightIntensities[0]"), static_cast<GLsizei>(maxLampLightCount), lightIntensities.data());
}

void DrawLampGlowMarkers(const std::vector<Light>& lights, Shader& sphereShader, GLuint lampGlowVAO, float nightFactor)
{
    glBindVertexArray(lampGlowVAO);
    for (const Light& light : lights)
    {
        const glm::mat4 lampModel =
            glm::translate(glm::mat4(1.0f), light.position) *
            glm::scale(glm::mat4(1.0f), glm::vec3(std::max(lampCoreSize, light.helperSize * 0.04f)));

        glUniform3f(glGetUniformLocation(sphereShader.ID, "color"), light.color.r, light.color.g, light.color.b);
        glUniform1f(glGetUniformLocation(sphereShader.ID, "alpha"), glm::clamp(nightFactor * light.intensity * 0.55f, 0.0f, 1.0f));
        glUniformMatrix4fv(glGetUniformLocation(sphereShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(lampModel));
        glDrawElements(GL_TRIANGLES, lampGlowIndexCount, GL_UNSIGNED_INT, 0);
    }
}

void DrawInteriorLightCubes(const std::vector<Light>& lights, Shader& lightShader, Camera& camera, GLuint cubeVAO, const glm::vec3& dirLightDirection)
{
    lightShader.Activate();
    camera.Matrix(lightShader, "camMatrix");
    glUniform3f(glGetUniformLocation(lightShader.ID, "dirLightDirection"), dirLightDirection.x, dirLightDirection.y, dirLightDirection.z);
    glBindVertexArray(cubeVAO);

    for (const Light& light : lights)
    {
        if (light.visualType != LightVisualType::Cube)
        {
            continue;
        }

        const glm::vec4 cubeColor(
            glm::clamp(light.color.r * (0.65f + light.intensity * 0.25f), 0.0f, 1.0f),
            glm::clamp(light.color.g * (0.65f + light.intensity * 0.25f), 0.0f, 1.0f),
            glm::clamp(light.color.b * (0.65f + light.intensity * 0.25f), 0.0f, 1.0f),
            0.95f);
        const glm::mat4 model =
            glm::translate(glm::mat4(1.0f), light.position) *
            glm::scale(glm::mat4(1.0f), glm::max(light.boxSize, glm::vec3(1.0f)));

        glUniformMatrix4fv(glGetUniformLocation(lightShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform4f(glGetUniformLocation(lightShader.ID, "lightColor"), cubeColor.x, cubeColor.y, cubeColor.z, cubeColor.w);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    glBindVertexArray(0);
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(width, height, "Proyecto", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    gladLoadGL();
    glViewport(0, 0, width, height);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    SetupSkyPanelStyle();
    ImGuiIO& imguiIo = ImGui::GetIO();
    imguiIo.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    imguiIo.Fonts->AddFontFromFileTTF("Texturas/Fonts/calibri.ttf", 18.0f);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    Shader shaderProgram("Shaders/default.vert", "Shaders/default.frag");
    Shader lightShader("Shaders/light.vert", "Shaders/light.frag");
    Shader sphereShader("Shaders/sphere.vert", "Shaders/sphere.frag");
    Shader overlayShader("Shaders/menu.vert", "Shaders/menu.frag");
    Texture sunTexture("Texturas/sun.jpg", "diffuse", 0);
    Texture moonTexture("Texturas/moon.jpg", "diffuse", 0);
    sphereShader.Activate();
    sunTexture.texUnit(sphereShader, "sphereTexture", 0);

    GLuint sunVAO, sunVBO, sunEBO;
    GLuint moonVAO, moonVBO, moonEBO;
    GLuint lampGlowVAO, lampGlowVBO, lampGlowEBO;
    GLuint lightCubeVAO, lightCubeVBO;
    GLuint overlayVAO, overlayVBO;
    glGenVertexArrays(1, &overlayVAO);
    glGenBuffers(1, &overlayVBO);
    glBindVertexArray(overlayVAO);
    glBindBuffer(GL_ARRAY_BUFFER, overlayVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(OverlayVertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(OverlayVertex), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    createSphere(sunVAO, sunVBO, sunEBO, 48, SUN_SIZE);
    createSphere(moonVAO, moonVBO, moonEBO, 36, MOON_SIZE);
    createSphere(lampGlowVAO, lampGlowVBO, lampGlowEBO, lampGlowSectors, lampGlowSize);
    createCube(lightCubeVAO, lightCubeVBO);

    Model model("modelos/city2.glb");
    Skybox skybox(
        GetSkyboxFacePaths(EnvironmentMode::Day),
        GetSkyboxFacePaths(EnvironmentMode::Night),
        "Shaders/skybox_cubemap.vert",
        "Shaders/skybox_cubemap.frag"
    );

    glm::vec3 modelCenter = model.GetCenter();
    float modelRadius = model.GetRadius();
    if (modelRadius < 1.0f) modelRadius = 1.0f;

    const float normalizationScale = targetSceneRadius / modelRadius;
    const glm::mat4 baseModelTransform =
        glm::scale(glm::mat4(1.0f), glm::vec3(normalizationScale)) *
        glm::translate(glm::mat4(1.0f), -modelCenter);

    EditorSceneData sceneData;
    Entity cityEntity;
    cityEntity.id = "city_root";
    cityEntity.name = "City";
    cityEntity.assetPath = "modelos/city.glb";
    cityEntity.pickRadius = targetSceneRadius;
    sceneData.entities.push_back(cityEntity);

    for (std::size_t i = 0; i < defaultLampLightPositions.size(); ++i)
    {
        Light light;
        light.id = "lamp_" + std::to_string(i);
        light.name = "Lamp " + std::to_string(i + 1);
        light.position = defaultLampLightPositions[i];
        light.color = defaultLampLightColor;
        light.radius = defaultLampLightRadius;
        light.intensity = defaultLampLightIntensity;
        light.helperSize = 7.5f;
        sceneData.lights.push_back(light);
    }

    SceneSerializer preloadSerializer;
    preloadSerializer.Load("scene_overrides.json", sceneData);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    Camera camera(width, height, glm::vec3(112.0f, -179.0f, 600.0f));
    camera.Orientation = glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f));
    camera.speed = walkSpeed;
    camera.flyMode = false;

    float lastFrame = 0.0f;
    float lastTitleUpdate = 0.0f;
    glm::vec3 lastTitlePosition = camera.Position;
    float timeOfDayAngle = 0.0f;
    float sunHeight = 0.0f;
    float dayFactor = 0.0f;
    float nightFactor = 0.0f;
    float manualTimeOfDay = 0.18f;
    SkyCloudSettings cloudSettings;
    glm::vec3 sunPos(0.0f);
    glm::vec3 moonPos(0.0f);
    bool isDay = true;
    glm::vec3 mainLightPos(0.0f);
    glm::vec3 mainLightColor(1.0f);
    float mainLightIntensity = 1.0f;
    glm::vec3 ambientColor(0.3f);
    float diffuseIntensity = 1.0f;
    float specularIntensity = 0.5f;
    glm::vec4 lightColor(1.0f);
    glm::vec3 skySunDirection(0.0f, 1.0f, 0.0f);
    ColliderManager colliderManager;
    CollisionSystem collisionSystem;

    Editor editor;
    EditorConfig editorConfig;
    editorConfig.window = window;
    editorConfig.camera = &camera;
    editorConfig.scene = &sceneData;
    editorConfig.sceneFilePath = "scene_overrides.json";
    editorConfig.lightFilePath = "lights_overrides.json";
    editorConfig.cameraFov = cameraFov;
    editorConfig.nearPlane = cameraNearPlane;
    editorConfig.farPlane = cameraFarPlane;
    editorConfig.viewportRenderer = [&](const EditorViewportRenderRequest& request)
    {
        Camera previewCamera = camera;
        previewCamera.width = request.width;
        previewCamera.height = request.height;
        previewCamera.Position = request.cameraPosition;
        previewCamera.cameraMatrix = request.projection * request.view;

        shaderProgram.Activate();
        glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), mainLightPos.x, mainLightPos.y, mainLightPos.z);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "moonPos"), moonPos.x, moonPos.y, moonPos.z);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "viewPos"), request.cameraPosition.x, request.cameraPosition.y, request.cameraPosition.z);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "ambientColor"), ambientColor.x, ambientColor.y, ambientColor.z);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "time"), lastFrame);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "dayFactor"), dayFactor);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "nightFactor"), nightFactor);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "isDay"), isDay ? 1.0f : 0.0f);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "diffuseIntensity"), diffuseIntensity);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "specularIntensity"), specularIntensity);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "sunHeight"), sunHeight);
        UploadLampLightUniforms(shaderProgram, sceneData.lights);

        for (const Entity& entity : sceneData.entities)
        {
            model.Draw(shaderProgram, previewCamera, BuildSceneModelTransform(entity, baseModelTransform));
        }

        DrawInteriorLightCubes(sceneData.lights, lightShader, previewCamera, lightCubeVAO, skySunDirection);
    };
    editor.Init(editorConfig);

    CollisionEditor collisionEditor;
    CollisionEditorConfig collisionEditorConfig;
    collisionEditorConfig.window = window;
    collisionEditorConfig.camera = &camera;
    collisionEditorConfig.manager = &colliderManager;
    collisionEditorConfig.collisionSystem = &collisionSystem;
    collisionEditorConfig.filePath = "colliders_overrides.json";
    collisionEditorConfig.placementPlaneOffset = walkEyeHeight;
    collisionEditorConfig.viewportRenderer = [&](const CollisionViewportRenderRequest& request)
    {
        Camera previewCamera = camera;
        previewCamera.width = request.width;
        previewCamera.height = request.height;
        previewCamera.Position = request.cameraPosition;
        previewCamera.cameraMatrix = request.projection * request.view;

        shaderProgram.Activate();
        glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), mainLightPos.x, mainLightPos.y, mainLightPos.z);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "moonPos"), moonPos.x, moonPos.y, moonPos.z);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "viewPos"), request.cameraPosition.x, request.cameraPosition.y, request.cameraPosition.z);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "ambientColor"), ambientColor.x, ambientColor.y, ambientColor.z);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "time"), lastFrame);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "dayFactor"), dayFactor);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "nightFactor"), nightFactor);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "isDay"), isDay ? 1.0f : 0.0f);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "diffuseIntensity"), diffuseIntensity);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "specularIntensity"), specularIntensity);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "sunHeight"), sunHeight);
        UploadLampLightUniforms(shaderProgram, sceneData.lights);

        for (const Entity& entity : sceneData.entities)
        {
            model.Draw(shaderProgram, previewCamera, BuildSceneModelTransform(entity, baseModelTransform));
        }

        DrawInteriorLightCubes(sceneData.lights, lightShader, previewCamera, lightCubeVAO, skySunDirection);
    };
    collisionEditorConfig.cameraFov = cameraFov;
    collisionEditorConfig.nearPlane = cameraNearPlane;
    collisionEditorConfig.farPlane = cameraFarPlane;
    collisionEditorConfig.worldInteractionBlocked = [&editor]()
    {
        return editor.IsActive();
    };
    collisionEditor.Init(collisionEditorConfig);

    glm::vec3 snappedStart;
    if (model.TrySnapToWalkableSurface(
        camera.Position, BuildSceneModelTransform(sceneData.entities.front(), baseModelTransform),
        walkProbeRadius, walkEyeHeight,
        walkMaxStepUp, walkMaxDropDown, walkMaxSlopeDegrees,
        snappedStart))
    {
        camera.Position = snappedStart;
        lastTitlePosition = camera.Position;
    }

    EnvironmentMode environmentMode = EnvironmentMode::Auto;
    EnvironmentMenuState environmentMenu;
    SkyPanelState skyPanel;
    bool menuCursorVisible = false;
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        editor.Update(deltaTime);
        collisionEditor.Update(deltaTime);

        bool shouldExit = false;
        if (!editor.IsActive() && !collisionEditor.IsActive())
        {
            HandleEnvironmentMenu(window, environmentMenu, environmentMode, shouldExit);
            HandleSkyPanel(window, skyPanel, environmentMode, manualTimeOfDay);
        }
        if (shouldExit)
        {
            glfwSetWindowShouldClose(window, true);
        }

        const bool wantsUiCursor = environmentMenu.open || skyPanel.open || editor.WantsCursor() || collisionEditor.WantsCursor();
        if (wantsUiCursor && !menuCursorVisible)
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            menuCursorVisible = true;
            camera.firstClick = true;
        }
        else if (!wantsUiCursor && menuCursorVisible)
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            menuCursorVisible = false;
            camera.firstClick = true;
        }

        timeOfDayAngle = currentFrame * dayNightSpeed;
        float sunAngleRad = timeOfDayAngle;

        sunHeight = std::sin(sunAngleRad);
        ApplyEnvironmentMode(environmentMode, manualTimeOfDay, sunAngleRad, sunHeight);
        isDay = sunHeight > 0.0f;

        const float skyBlendFactor = environmentMode == EnvironmentMode::Day
            ? 0.0f
            : (environmentMode == EnvironmentMode::Night ? 1.0f : ComputeSkyBlendFactor(sunHeight));
        skybox.SetBlendFactor(skyBlendFactor);
        skybox.SetCloudSettings(cloudSettings);

        dayFactor = glm::clamp(sunHeight + 0.2f, 0.05f, 1.0f);
        nightFactor = glm::clamp(-sunHeight + 0.1f, 0.0f, 1.0f);

        float sunHorizontal = std::cos(sunAngleRad);
        float sunVertical = sunHeight;

        if (sunVertical > 0.0f) {
            sunPos = glm::vec3(
                sunHorizontal * celestialOrbitRadius,
                sunVertical * maxSunHeight,
                std::sin(sunAngleRad) * celestialOrbitRadius * 0.5f
            );
        }
        else {
            sunPos = glm::vec3(
                sunHorizontal * celestialOrbitRadius,
                -500.0f,
                std::sin(sunAngleRad) * celestialOrbitRadius * 0.5f
            );
        }

        float moonAngleRad = sunAngleRad + 3.14159f;
        float moonHorizontal = std::cos(moonAngleRad);
        float moonVertical = std::sin(moonAngleRad);

        if (moonVertical > 0.0f) {
            moonPos = glm::vec3(
                moonHorizontal * celestialOrbitRadius * 0.8f,
                moonVertical * maxSunHeight * maxMoonHeightFactor,
                std::sin(moonAngleRad) * celestialOrbitRadius * 0.4f
            );
        }
        else {
            moonPos = glm::vec3(
                moonHorizontal * celestialOrbitRadius * 0.8f,
                -300.0f,
                std::sin(moonAngleRad) * celestialOrbitRadius * 0.4f
            );
        }

        const float dayLightBlend = glm::smoothstep(-0.14f, 0.18f, sunHeight);
        const float sunWarmBlend = glm::smoothstep(-0.04f, 0.32f, sunHeight);
        const float sunNoonBlend = glm::smoothstep(0.18f, 0.82f, sunHeight);
        const float sunPresence = glm::smoothstep(-0.08f, 0.75f, sunHeight);
        const float moonPresence = glm::smoothstep(0.08f, -0.88f, sunHeight);
        const float moonHeight = glm::clamp(std::abs(moonVertical), 0.0f, 1.0f);

        const glm::vec3 sunWarmColor = glm::mix(
            glm::vec3(0.92f, 0.46f, 0.28f),
            glm::vec3(1.0f, 0.76f, 0.56f),
            sunWarmBlend
        );
        const glm::vec3 sunLightColor = glm::mix(
            sunWarmColor,
            glm::vec3(1.0f, 0.98f, 0.92f),
            sunNoonBlend
        );
        const float sunLightIntensity = glm::mix(0.16f, 1.18f, sunPresence);
        const glm::vec3 sunAmbientColor = glm::mix(
            glm::vec3(0.08f, 0.07f, 0.10f),
            glm::vec3(0.35f, 0.38f, 0.42f),
            glm::smoothstep(-0.02f, 0.72f, sunHeight)
        );
        const float sunDiffuseIntensity = glm::mix(0.22f, 1.0f, sunPresence);
        const float sunSpecularIntensity = glm::mix(0.12f, 0.60f, sunPresence);

        const glm::vec3 moonLightColor = glm::mix(
            glm::vec3(0.50f, 0.56f, 0.74f),
            glm::vec3(0.65f, 0.70f, 0.90f),
            moonHeight
        );
        const float moonLightIntensity = glm::mix(0.14f, 0.34f, moonPresence * moonHeight);
        const glm::vec3 moonAmbientColor = glm::mix(
            glm::vec3(0.05f, 0.06f, 0.08f),
            glm::vec3(0.08f, 0.10f, 0.15f),
            glm::smoothstep(-0.90f, -0.10f, sunHeight)
        );
        const float moonDiffuseIntensity = glm::mix(0.22f, 0.35f, moonPresence);
        const float moonSpecularIntensity = glm::mix(0.10f, 0.15f, moonPresence);

        mainLightPos = glm::mix(moonPos, sunPos, dayLightBlend);
        skySunDirection = glm::normalize(glm::vec3(
            std::cos(sunAngleRad),
            std::sin(sunAngleRad),
            std::sin(sunAngleRad) * 0.5f
        ));
        mainLightColor = glm::mix(moonLightColor, sunLightColor, dayLightBlend);
        mainLightIntensity = glm::mix(moonLightIntensity, sunLightIntensity, dayLightBlend);
        ambientColor = glm::mix(moonAmbientColor, sunAmbientColor, dayLightBlend);
        diffuseIntensity = glm::mix(moonDiffuseIntensity, sunDiffuseIntensity, dayLightBlend);
        specularIntensity = glm::mix(moonSpecularIntensity, sunSpecularIntensity, dayLightBlend);

        lightColor = glm::vec4(mainLightColor * mainLightIntensity, 1.0f);

        const glm::vec3 nightSkyColor(0.03f, 0.04f, 0.08f);
        const glm::vec3 twilightSkyColor(0.88f, 0.50f, 0.34f);
        const glm::vec3 daySkyColor(0.46f, 0.74f, 0.98f);
        const float twilightBlend = glm::smoothstep(-0.22f, 0.18f, sunHeight);
        const float dayBlend = glm::smoothstep(0.08f, 0.72f, sunHeight);
        glm::vec3 skyColor = glm::mix(nightSkyColor, twilightSkyColor, twilightBlend);
        skyColor = glm::mix(skyColor, daySkyColor, dayBlend);

        if (sunHeight < 0.15f)
        {
            const float moonGlow = glm::smoothstep(-0.65f, 0.05f, -sunHeight) * glm::clamp(std::abs(moonVertical), 0.0f, 1.0f);
            skyColor += mainLightColor * (0.018f + moonGlow * 0.035f);
        }

        glClearColor(skyColor.x, skyColor.y, skyColor.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const glm::mat4 sceneModelTransform = BuildSceneModelTransform(sceneData.entities.front(), baseModelTransform);
        const glm::vec3 prevPos = camera.Position;
        if (!environmentMenu.open && !skyPanel.open && !editor.IsActive() && !collisionEditor.IsActive())
        {
            if (environmentMode == EnvironmentMode::Manual && !skyPanel.open)
            {
                const bool decreaseTime = glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_4) == GLFW_PRESS;
                const bool increaseTime = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_6) == GLFW_PRESS;
                if (decreaseTime)
                    manualTimeOfDay = WrapTimeOfDay(manualTimeOfDay - deltaTime * 0.08f);
                if (increaseTime)
                    manualTimeOfDay = WrapTimeOfDay(manualTimeOfDay + deltaTime * 0.08f);
            }

            camera.Inputs(window, deltaTime);
        }

        if (!camera.flyMode)
        {
            glm::vec3 snapped;
            if (model.TrySnapToWalkableSurface(
                camera.Position, sceneModelTransform,
                walkProbeRadius, walkEyeHeight,
                walkMaxStepUp, walkMaxDropDown, walkMaxSlopeDegrees,
                snapped))
                camera.Position = snapped;
            else if (model.TrySnapToWalkableSurface(
                prevPos, sceneModelTransform,
                walkProbeRadius, walkEyeHeight,
                walkMaxStepUp, walkMaxDropDown, walkMaxSlopeDegrees,
                snapped))
                camera.Position = snapped;
            else
                camera.Position = prevPos;

            const bool blocked =
                camera.Position.x >= blockedZoneMin.x && camera.Position.x <= blockedZoneMax.x &&
                camera.Position.y >= blockedZoneMin.y && camera.Position.y <= blockedZoneMax.y &&
                camera.Position.z >= blockedZoneMin.z && camera.Position.z <= blockedZoneMax.z;
            if (blocked) camera.Position = prevPos;
        }

        camera.Position = collisionSystem.ResolveCameraPosition(
            colliderManager.GetColliders(),
            prevPos,
            camera.Position,
            cameraCollisionRadius);

        camera.updateMatrix(cameraFov, cameraNearPlane, cameraFarPlane);
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(cameraFov), (float)width / height, cameraNearPlane, cameraFarPlane);

        shaderProgram.Activate();
        glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), mainLightPos.x, mainLightPos.y, mainLightPos.z);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "moonPos"), moonPos.x, moonPos.y, moonPos.z);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "viewPos"), camera.Position.x, camera.Position.y, camera.Position.z);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "ambientColor"), ambientColor.x, ambientColor.y, ambientColor.z);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "time"), currentFrame);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "dayFactor"), dayFactor);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "nightFactor"), nightFactor);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "isDay"), isDay ? 1.0f : 0.0f);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "diffuseIntensity"), diffuseIntensity);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "specularIntensity"), specularIntensity);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "sunHeight"), sunHeight);
        UploadLampLightUniforms(shaderProgram, sceneData.lights);

        for (const Entity& entity : sceneData.entities)
        {
            model.Draw(shaderProgram, camera, BuildSceneModelTransform(entity, baseModelTransform));
        }

        DrawInteriorLightCubes(sceneData.lights, lightShader, camera, lightCubeVAO, skySunDirection);

        if (nightFactor > 0.02f) {
            sphereShader.Activate();
            glUniform1f(glGetUniformLocation(sphereShader.ID, "useTexture"), 0.0f);
            glUniform1f(glGetUniformLocation(sphereShader.ID, "unlit"), 1.0f);
            glUniform3f(glGetUniformLocation(sphereShader.ID, "lightPos"), camera.Position.x, camera.Position.y, camera.Position.z);
            glUniform3f(glGetUniformLocation(sphereShader.ID, "viewPos"), camera.Position.x, camera.Position.y, camera.Position.z);
            glUniform1f(glGetUniformLocation(sphereShader.ID, "isDay"), 1.0f);
            glUniform1f(glGetUniformLocation(sphereShader.ID, "sunHeight"), sunHeight);
            glUniformMatrix4fv(glGetUniformLocation(sphereShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(sphereShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

            DrawLampGlowMarkers(sceneData.lights, sphereShader, lampGlowVAO, nightFactor);
            glUniform1f(glGetUniformLocation(sphereShader.ID, "unlit"), 0.0f);
            glUniform1f(glGetUniformLocation(sphereShader.ID, "alpha"), 1.0f);
        }

        if (sunPos.y > -200.0f && isDay) {
            sphereShader.Activate();
            sunTexture.Bind();
            glUniform1f(glGetUniformLocation(sphereShader.ID, "useTexture"), 1.0f);
            glUniform1f(glGetUniformLocation(sphereShader.ID, "unlit"), 0.0f);
            glUniform1f(glGetUniformLocation(sphereShader.ID, "alpha"), 1.0f);
            glm::mat4 sunModel = glm::translate(glm::mat4(1.0f), sunPos);
            glUniformMatrix4fv(glGetUniformLocation(sphereShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(sunModel));
            glUniformMatrix4fv(glGetUniformLocation(sphereShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(sphereShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            glUniform3f(glGetUniformLocation(sphereShader.ID, "color"), 1.0f, 0.85f, 0.45f);
            glUniform3f(glGetUniformLocation(sphereShader.ID, "lightPos"), mainLightPos.x, mainLightPos.y, mainLightPos.z);
            glUniform3f(glGetUniformLocation(sphereShader.ID, "viewPos"), camera.Position.x, camera.Position.y, camera.Position.z);
            glUniform1f(glGetUniformLocation(sphereShader.ID, "isDay"), 1.0f);
            glUniform1f(glGetUniformLocation(sphereShader.ID, "sunHeight"), sunHeight);

            glBindVertexArray(sunVAO);
            glDrawElements(GL_TRIANGLES, 48 * 48 * 6, GL_UNSIGNED_INT, 0);
            sunTexture.Unbind();
        }

        if (moonPos.y > -200.0f && !isDay) {
            sphereShader.Activate();
            moonTexture.Bind();
            glUniform1f(glGetUniformLocation(sphereShader.ID, "useTexture"), 1.0f);
            glUniform1f(glGetUniformLocation(sphereShader.ID, "unlit"), 0.0f);
            glUniform1f(glGetUniformLocation(sphereShader.ID, "alpha"), 1.0f);
            glm::mat4 moonModel =
                glm::translate(glm::mat4(1.0f), moonPos) *
                glm::inverse(glm::mat4(glm::mat3(view)));
            glUniformMatrix4fv(glGetUniformLocation(sphereShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(moonModel));
            glUniformMatrix4fv(glGetUniformLocation(sphereShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(sphereShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            glUniform3f(glGetUniformLocation(sphereShader.ID, "color"), 0.75f, 0.80f, 0.95f);
            glUniform3f(glGetUniformLocation(sphereShader.ID, "lightPos"), mainLightPos.x, mainLightPos.y, mainLightPos.z);
            glUniform3f(glGetUniformLocation(sphereShader.ID, "viewPos"), camera.Position.x, camera.Position.y, camera.Position.z);
            glUniform1f(glGetUniformLocation(sphereShader.ID, "isDay"), 0.0f);
            glUniform1f(glGetUniformLocation(sphereShader.ID, "sunHeight"), sunHeight);

            glBindVertexArray(moonVAO);
            glDrawElements(GL_TRIANGLES, 36 * 36 * 6, GL_UNSIGNED_INT, 0);
            moonTexture.Unbind();
        }

        if (!useFastRenderMode)
            skybox.Draw(camera, cameraFov, cameraNearPlane, cameraFarPlane, currentFrame, sunHeight, skySunDirection);

        DrawEnvironmentMenu(environmentMenu, environmentMode, manualTimeOfDay, overlayShader, overlayVAO, overlayVBO);
        DrawSkyControlImGui(skyPanel, environmentMode, manualTimeOfDay, cloudSettings);
        editor.Render();
        collisionEditor.Render();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (showCoordinatesInWindowTitle && currentFrame - lastTitleUpdate >= 0.1f)
        {
            const glm::vec3 normPos = glm::clamp(camera.Position / targetSceneRadius, glm::vec3(-1.0f), glm::vec3(1.0f));

            const std::string title = BuildEnvironmentTitle(environmentMenu, environmentMode, manualTimeOfDay, normPos);
            glfwSetWindowTitle(window, title.c_str());
            lastTitleUpdate = currentFrame;
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &sunVAO);
    glDeleteBuffers(1, &sunVBO);
    glDeleteBuffers(1, &sunEBO);
    glDeleteVertexArrays(1, &moonVAO);
    glDeleteBuffers(1, &moonVBO);
    glDeleteBuffers(1, &moonEBO);
    glDeleteVertexArrays(1, &lampGlowVAO);
    glDeleteBuffers(1, &lampGlowVBO);
    glDeleteBuffers(1, &lampGlowEBO);
    glDeleteVertexArrays(1, &lightCubeVAO);
    glDeleteBuffers(1, &lightCubeVBO);
    sunTexture.Delete();
    moonTexture.Delete();
    glDeleteVertexArrays(1, &overlayVAO);
    glDeleteBuffers(1, &overlayVBO);
    editor.Shutdown();
    collisionEditor.Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
