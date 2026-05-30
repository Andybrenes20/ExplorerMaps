#define _CRT_SECURE_NO_WARNINGS

//------- Ignore this ----------
#include<algorithm>
#include<array>
#include<cfloat>
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
// CICLO MUY RAPIDO PARA VIDEO
const float dayNightSpeed = 0.20f;
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

enum class AppScreen
{
    MainMenu,
    Loading,
    World
};

enum class LoadingDestination
{
    MainMenu,
    World
};

enum class MainMenuAction
{
    None,
    StartWorld,
    Quit
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

struct InteractionState
{
    SceneSelection target;
    std::string targetName;
    std::string message;
    float messageUntil = 0.0f;
    bool interactWasDown = false;
};

struct WalkAnimationState
{
    float phase = 0.0f;
    float amount = 0.0f;
};

struct DrivableCarState
{
    glm::vec3 position = glm::vec3(150.0f, -179.0f, 620.0f);
    float yawDegrees = -90.0f;
    float visualYawDegrees = -90.0f;
    float speed = 0.0f;
    float steeringAmount = 0.0f;
    float cameraYawOffsetDegrees = 0.0f;
    float cameraHeightOffset = 0.0f;
    bool groundHeightInitialized = false;
    bool driving = false;
    bool interactWasDown = false;
};

struct GameplayGamepadInput
{
    bool connected = false;
    float leftX = 0.0f;
    float leftY = 0.0f;
    float rightX = 0.0f;
    float rightY = 0.0f;
    float rightTrigger = 0.0f;
    float leftTrigger = 0.0f;
    bool interactDown = false;
};

struct MainMenuState
{
    int selection = 0;
    bool mouseWasDown = false;
    bool upWasDown = false;
    bool downWasDown = false;
    bool enterWasDown = false;
    bool gamepadUpWasDown = false;
    bool gamepadDownWasDown = false;
    bool gamepadAcceptWasDown = false;
    bool gamepadCancelWasDown = false;
};

constexpr int kEnvironmentMenuItemCount = 4;
constexpr int kEnvironmentMenuExitIndex = 3;
constexpr float kManualTimeStep = 1.0f / 24.0f;
constexpr int kSkyPanelItemCount = 3;

bool IsInsideRect(double px, double py, float x, float y, float w, float h)
{
    return px >= x && px <= (x + w) && py >= y && py <= (y + h);
}

float ApplyStickDeadzone(float value, float deadzone = 0.18f)
{
    if (std::abs(value) <= deadzone)
        return 0.0f;

    const float sign = value < 0.0f ? -1.0f : 1.0f;
    return sign * glm::clamp((std::abs(value) - deadzone) / (1.0f - deadzone), 0.0f, 1.0f);
}

float NormalizeTriggerAxis(float value)
{
    return glm::clamp((value + 1.0f) * 0.5f, 0.0f, 1.0f);
}

GameplayGamepadInput ReadGameplayGamepadInput()
{
    GameplayGamepadInput input;
    if (!glfwJoystickIsGamepad(GLFW_JOYSTICK_1))
        return input;

    GLFWgamepadstate state;
    if (!glfwGetGamepadState(GLFW_JOYSTICK_1, &state))
        return input;

    input.connected = true;
    input.leftX = ApplyStickDeadzone(state.axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
    input.leftY = ApplyStickDeadzone(state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
    input.rightX = ApplyStickDeadzone(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]);
    input.rightY = ApplyStickDeadzone(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
    input.rightTrigger = NormalizeTriggerAxis(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER]);
    input.leftTrigger = NormalizeTriggerAxis(state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER]);
    input.interactDown = state.buttons[GLFW_GAMEPAD_BUTTON_X] == GLFW_PRESS;
    return input;
}

bool IsGameplayMovementPressed(GLFWwindow* window, const GameplayGamepadInput& gamepad)
{
    return glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS ||
        std::abs(gamepad.leftX) > 0.01f ||
        std::abs(gamepad.leftY) > 0.01f;
}

float IntersectInteractionSphere(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const glm::vec3& center, float radius);

glm::vec3 CarForward(float yawDegrees)
{
    const float yawRadians = glm::radians(yawDegrees);
    return glm::normalize(glm::vec3(std::cos(yawRadians), 0.0f, std::sin(yawRadians)));
}

float MoveAngleTowards(float currentDegrees, float targetDegrees, float maxDeltaDegrees)
{
    float delta = std::fmod(targetDegrees - currentDegrees + 540.0f, 360.0f) - 180.0f;
    delta = glm::clamp(delta, -maxDeltaDegrees, maxDeltaDegrees);
    return currentDegrees + delta;
}

glm::vec3 ComputeCarLocalAnchorOffset(const Model& carModel)
{
    const glm::vec3 boundsMin = carModel.GetBoundsMin();
    const glm::vec3 boundsMax = carModel.GetBoundsMax();
    const glm::vec3 boundsCenter = (boundsMin + boundsMax) * 0.5f;
    return glm::vec3(-boundsCenter.x, -boundsMin.y, -boundsCenter.z);
}

glm::mat4 BuildCarTransform(const DrivableCarState& car, const glm::vec3& localAnchorOffset)
{
    constexpr float carVisualYawOffset = 90.0f;
    return glm::translate(glm::mat4(1.0f), car.position) *
        glm::rotate(glm::mat4(1.0f), glm::radians(car.visualYawDegrees + carVisualYawOffset), glm::vec3(0.0f, 1.0f, 0.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(18.0f)) *
        glm::translate(glm::mat4(1.0f), localAnchorOffset);
}

void DrawDrivableCar(
    Shader& shader,
    Camera& camera,
    Model& carModel,
    const DrivableCarState& car,
    const glm::vec3& localAnchorOffset)
{
    glUniform1f(glGetUniformLocation(shader.ID, "objectLightBoost"), 1.0f);
    carModel.Draw(shader, camera, BuildCarTransform(car, localAnchorOffset));
    glUniform1f(glGetUniformLocation(shader.ID, "objectLightBoost"), 0.0f);
}

bool IsLookingAtCar(const Camera& camera, const DrivableCarState& car)
{
    const glm::vec3 rayOrigin = camera.Position;
    const glm::vec3 rayDirection = glm::normalize(camera.Orientation);
    const float distance = IntersectInteractionSphere(rayOrigin, rayDirection, car.position + glm::vec3(0.0f, 9.0f, 0.0f), 34.0f);
    return distance < 160.0f;
}

void UpdateDrivableCar(
    GLFWwindow* window,
    Camera& camera,
    DrivableCarState& car,
    const Model& cityModel,
    const glm::mat4& sceneModelTransform,
    InteractionState& interaction,
    const GameplayGamepadInput& gamepad,
    float currentFrame,
    float deltaTime)
{
    const bool interactDown = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS || gamepad.interactDown;
    const bool interactPressed = interactDown && !car.interactWasDown;
    const bool lookingAtCar = IsLookingAtCar(camera, car);

    if (interactPressed)
    {
        if (car.driving)
        {
            car.driving = false;
            camera.Position = car.position - CarForward(car.yawDegrees) * 34.0f + glm::vec3(0.0f, 10.0f, 0.0f);
            interaction.message = "SALISTE DEL COCHE";
            interaction.messageUntil = currentFrame + 1.6f;
        }
        else if (lookingAtCar)
        {
            car.driving = true;
            camera.flyMode = false;
            camera.firstClick = true;
            interaction.message = "CONDUCIENDO: WASD, SPACE FRENO, E SALIR";
            interaction.messageUntil = currentFrame + 2.4f;
        }
    }
    car.interactWasDown = interactDown;

    if (!car.driving)
    {
        if (lookingAtCar)
        {
            interaction.targetName = "Coche";
            interaction.message = "E  CONDUCIR COCHE";
            interaction.messageUntil = currentFrame + 0.1f;
        }
        return;
    }

    float acceleration = 0.0f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        acceleration += 95.0f;
    if (gamepad.rightTrigger > 0.02f)
        acceleration += 95.0f * gamepad.rightTrigger;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        acceleration -= 75.0f;
    if (gamepad.leftTrigger > 0.02f)
        acceleration -= 75.0f * gamepad.leftTrigger;

    car.speed += acceleration * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        car.speed = glm::mix(car.speed, 0.0f, glm::clamp(deltaTime * 7.5f, 0.0f, 1.0f));
    else if (std::abs(acceleration) < 0.001f)
        car.speed = glm::mix(car.speed, 0.0f, glm::clamp(deltaTime * 1.8f, 0.0f, 1.0f));

    car.speed = glm::clamp(car.speed, -45.0f, 130.0f);

    float targetSteering =
        (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS ? 1.0f : 0.0f) -
        (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ? 1.0f : 0.0f);
    if (std::abs(gamepad.leftX) > 0.01f)
        targetSteering = gamepad.leftX;

    targetSteering *= 0.90f;

    const float steeringResponse = std::abs(targetSteering) > std::abs(car.steeringAmount) ? 7.5f : 9.0f;
    car.steeringAmount = glm::mix(
        car.steeringAmount,
        glm::clamp(targetSteering, -1.0f, 1.0f),
        glm::clamp(deltaTime * steeringResponse, 0.0f, 1.0f));

    const float speedAbs = std::abs(car.speed);
    const float steeringGrip = glm::smoothstep(3.0f, 22.0f, speedAbs);
    const float highSpeedStability = glm::mix(1.0f, 0.50f, glm::smoothstep(58.0f, 130.0f, speedAbs));
    const float maxYawRateDegrees = glm::mix(68.0f, 38.0f, glm::smoothstep(45.0f, 130.0f, speedAbs));
    const float yawRateDegrees = car.steeringAmount * steeringGrip * highSpeedStability * maxYawRateDegrees;
    car.yawDegrees += yawRateDegrees * deltaTime * (car.speed >= 0.0f ? 1.0f : -1.0f);
    car.visualYawDegrees = MoveAngleTowards(car.visualYawDegrees, car.yawDegrees, 48.0f * deltaTime);

    const glm::vec3 previousCarPosition = car.position;
    car.position += CarForward(car.yawDegrees) * car.speed * deltaTime;

    glm::vec3 snappedCarPosition;
    if (cityModel.TrySnapToWalkableSurface(
        car.position + glm::vec3(0.0f, 10.0f, 0.0f),
        sceneModelTransform,
        18.0f,
        10.0f,
        18.0f,
        55.0f,
        walkMaxSlopeDegrees,
        snappedCarPosition))
    {
        const float targetGroundY = snappedCarPosition.y - 10.0f;
        if (!car.groundHeightInitialized)
        {
            car.position.y = targetGroundY;
            car.groundHeightInitialized = true;
        }
        else
        {
            car.position.y = glm::mix(car.position.y, targetGroundY, glm::clamp(deltaTime * 8.0f, 0.0f, 1.0f));
        }
    }
    else
    {
        car.position.x = previousCarPosition.x;
        car.position.z = previousCarPosition.z;
        car.speed = 0.0f;
    }

    if (std::abs(gamepad.rightX) > 0.01f)
        car.cameraYawOffsetDegrees = glm::clamp(car.cameraYawOffsetDegrees + gamepad.rightX * 145.0f * deltaTime, -135.0f, 135.0f);
    else
        car.cameraYawOffsetDegrees = glm::mix(car.cameraYawOffsetDegrees, 0.0f, glm::clamp(deltaTime * 1.8f, 0.0f, 1.0f));

    if (std::abs(gamepad.rightY) > 0.01f)
        car.cameraHeightOffset = glm::clamp(car.cameraHeightOffset - gamepad.rightY * 34.0f * deltaTime, -12.0f, 18.0f);
    else
        car.cameraHeightOffset = glm::mix(car.cameraHeightOffset, 0.0f, glm::clamp(deltaTime * 1.8f, 0.0f, 1.0f));

    const glm::vec3 cameraForward = CarForward(car.yawDegrees + car.cameraYawOffsetDegrees);
    const glm::vec3 cameraTarget = car.position + glm::vec3(0.0f, 16.0f + car.cameraHeightOffset * 0.25f, 0.0f);
    const glm::vec3 desiredCameraPosition = cameraTarget - cameraForward * 52.0f + glm::vec3(0.0f, 24.0f + car.cameraHeightOffset, 0.0f);
    camera.Position = glm::mix(camera.Position, desiredCameraPosition, glm::clamp(deltaTime * 8.0f, 0.0f, 1.0f));
    camera.Orientation = glm::normalize(cameraTarget - camera.Position);
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

bool FileExists(const char* path)
{
    std::error_code error;
    return fs::exists(path, error);
}

void LoadApplicationFont(ImGuiIO& imguiIo)
{
    const char* fontPaths[] =
    {
        "Texturas/Fonts/calibri.ttf",
        "ExplorerMaps/Texturas/Fonts/calibri.ttf",
        "../ExplorerMaps/Texturas/Fonts/calibri.ttf"
    };

    for (const char* fontPath : fontPaths)
    {
        if (FileExists(fontPath))
        {
            imguiIo.Fonts->AddFontFromFileTTF(fontPath, 18.0f);
            return;
        }
    }

    imguiIo.Fonts->AddFontDefault();
}

void SetWorkingDirectoryFromExecutable(const char* executablePath)
{
    if (executablePath == nullptr || executablePath[0] == '\0')
        return;

    std::error_code error;
    const fs::path executable = fs::absolute(executablePath, error);
    if (error)
        return;

    const fs::path executableDirectory = executable.parent_path();
    if (!executableDirectory.empty())
        fs::current_path(executableDirectory, error);
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
        const char* option3 = menu.selection == 3 ? "> INICIO" : "  INICIO";
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

void AddOverlayLineRect(std::vector<OverlayVertex>& vertices, float x0, float y0, float x1, float y1, float thickness, const glm::vec4& color)
{
    if (std::abs(x1 - x0) >= std::abs(y1 - y0))
    {
        const float x = std::min(x0, x1);
        AddOverlayRect(vertices, x, y0 - thickness * 0.5f, std::abs(x1 - x0), thickness, color);
    }
    else
    {
        const float y = std::min(y0, y1);
        AddOverlayRect(vertices, x0 - thickness * 0.5f, y, thickness, std::abs(y1 - y0), color);
    }
}

float IntersectInteractionSphere(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const glm::vec3& center, float radius)
{
    const glm::vec3 offset = rayOrigin - center;
    const float b = 2.0f * glm::dot(offset, rayDirection);
    const float c = glm::dot(offset, offset) - radius * radius;
    const float discriminant = b * b - 4.0f * c;
    if (discriminant < 0.0f)
        return FLT_MAX;

    const float root = std::sqrt(discriminant);
    const float t0 = (-b - root) * 0.5f;
    const float t1 = (-b + root) * 0.5f;
    if (t0 > 0.0f)
        return t0;
    return t1 > 0.0f ? t1 : FLT_MAX;
}

std::string SelectionDisplayName(const EditorSceneData& sceneData, const SceneSelection& selection)
{
    if (selection.type == SceneObjectType::Light && selection.index >= 0 && selection.index < static_cast<int>(sceneData.lights.size()))
        return sceneData.lights[selection.index].name;
    if (selection.type == SceneObjectType::Helper && selection.index >= 0 && selection.index < static_cast<int>(sceneData.helpers.size()))
        return sceneData.helpers[selection.index].name;
    if (selection.type == SceneObjectType::Entity && selection.index >= 0 && selection.index < static_cast<int>(sceneData.entities.size()))
        return sceneData.entities[selection.index].name;
    return "";
}

void UpdateInteractionState(
    GLFWwindow* window,
    const Camera& camera,
    const EditorSceneData& sceneData,
    InteractionState& interaction,
    const GameplayGamepadInput& gamepad,
    float currentFrame)
{
    const float maxInteractDistance = 180.0f;
    const glm::vec3 rayOrigin = camera.Position;
    const glm::vec3 rayDirection = glm::normalize(camera.Orientation);
    SceneSelection bestSelection;
    float bestDistance = maxInteractDistance;

    for (int i = 0; i < static_cast<int>(sceneData.lights.size()); ++i)
    {
        const Light& light = sceneData.lights[i];
        const float radius = std::max(light.helperSize, 10.0f);
        const float distance = IntersectInteractionSphere(rayOrigin, rayDirection, light.position, radius);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestSelection.type = SceneObjectType::Light;
            bestSelection.index = i;
        }
    }

    for (int i = 0; i < static_cast<int>(sceneData.helpers.size()); ++i)
    {
        const Helper& helper = sceneData.helpers[i];
        const float radius = std::max(MaxComponent(helper.scale), 10.0f);
        const float distance = IntersectInteractionSphere(rayOrigin, rayDirection, helper.position, radius);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestSelection.type = SceneObjectType::Helper;
            bestSelection.index = i;
        }
    }

    interaction.target = bestSelection;
    interaction.targetName = SelectionDisplayName(sceneData, bestSelection);

    const bool interactDown = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS || gamepad.interactDown;
    if (interactDown && !interaction.interactWasDown && interaction.target.IsValid())
    {
        interaction.message = "INTERACCION: " + interaction.targetName;
        interaction.messageUntil = currentFrame + 2.0f;
    }
    interaction.interactWasDown = interactDown;
}

void UpdateWalkAnimation(WalkAnimationState& walkAnimation, bool walking, float deltaTime)
{
    const float targetAmount = walking ? 1.0f : 0.0f;
    walkAnimation.amount = glm::mix(walkAnimation.amount, targetAmount, glm::clamp(deltaTime * 8.0f, 0.0f, 1.0f));
    if (walkAnimation.amount > 0.01f)
        walkAnimation.phase += deltaTime * 8.0f;
}

glm::vec3 ComputeWalkBobOffset(const Camera& camera, const WalkAnimationState& walkAnimation)
{
    if (walkAnimation.amount <= 0.001f)
        return glm::vec3(0.0f);

    glm::vec3 right = glm::cross(camera.Orientation, camera.Up);
    if (glm::length(right) < 0.0001f)
        right = glm::vec3(1.0f, 0.0f, 0.0f);
    right = glm::normalize(right);

    const float vertical = std::abs(std::sin(walkAnimation.phase)) * 0.55f * walkAnimation.amount;
    const float horizontal = std::sin(walkAnimation.phase * 0.5f) * 0.22f * walkAnimation.amount;
    return camera.Up * vertical + right * horizontal;
}

void DrawPlayerHud(const InteractionState& interaction, const WalkAnimationState& walkAnimation, float currentFrame, Shader& overlayShader, GLuint overlayVAO, GLuint overlayVBO)
{
    std::vector<OverlayVertex> vertices;
    const float bobX = std::sin(walkAnimation.phase * 0.5f) * 7.0f * walkAnimation.amount;
    const float bobY = std::abs(std::sin(walkAnimation.phase)) * 8.0f * walkAnimation.amount;
    const float cx = width * 0.5f + bobX;
    const float cy = height * 0.5f + bobY;

    const bool hasTarget = interaction.target.IsValid();
    const glm::vec4 reticleColor = hasTarget
        ? glm::vec4(1.0f, 0.82f, 0.24f, 0.95f)
        : glm::vec4(0.86f, 0.92f, 0.96f, 0.78f);
    const glm::vec4 shadow(0.0f, 0.0f, 0.0f, 0.35f);

    AddOverlayRect(vertices, cx - 3.0f, cy - 3.0f, 6.0f, 6.0f, shadow);
    AddOverlayRect(vertices, cx - 2.0f, cy - 2.0f, 4.0f, 4.0f, reticleColor);
    AddOverlayLineRect(vertices, cx - 22.0f, cy, cx - 8.0f, cy, 2.0f, reticleColor);
    AddOverlayLineRect(vertices, cx + 8.0f, cy, cx + 22.0f, cy, 2.0f, reticleColor);
    AddOverlayLineRect(vertices, cx, cy - 22.0f, cx, cy - 8.0f, 2.0f, reticleColor);
    AddOverlayLineRect(vertices, cx, cy + 8.0f, cx, cy + 22.0f, 2.0f, reticleColor);

    if (walkAnimation.amount > 0.05f)
    {
        const float stride = std::abs(std::sin(walkAnimation.phase)) * walkAnimation.amount;
        AddOverlayLineRect(vertices, cx - 70.0f + stride * 18.0f, cy + 86.0f, cx - 32.0f, cy + 34.0f, 5.0f, glm::vec4(0.02f, 0.02f, 0.025f, 0.22f));
        AddOverlayLineRect(vertices, cx + 70.0f - stride * 18.0f, cy + 86.0f, cx + 32.0f, cy + 34.0f, 5.0f, glm::vec4(0.02f, 0.02f, 0.025f, 0.22f));
    }

    if (hasTarget)
    {
        AddOverlayText(vertices, "E  INTERACTUAR", cx - 118.0f, cy + 54.0f, 0.62f, reticleColor);
        AddOverlayText(vertices, interaction.targetName, cx - 88.0f, cy + 82.0f, 0.52f, glm::vec4(0.78f, 0.84f, 0.90f, 0.88f));
    }

    if (!interaction.message.empty() && currentFrame < interaction.messageUntil)
        AddOverlayText(vertices, interaction.message, 52.0f, height - 84.0f, 0.72f, glm::vec4(1.0f, 0.86f, 0.34f, 0.94f));

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

        const char* labels[kEnvironmentMenuItemCount] = { "DIA", "NOCHE", "AUTO", "SALIR AL INICIO" };
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

void DrawMainMenuOverlay(const MainMenuState& menu, Shader& overlayShader, GLuint overlayVAO, GLuint overlayVBO)
{
    std::vector<OverlayVertex> vertices;
    const glm::vec4 bgTop(0.015f, 0.020f, 0.030f, 1.0f);
    const glm::vec4 bgBottom(0.030f, 0.038f, 0.050f, 1.0f);
    const glm::vec4 panelColor(0.020f, 0.026f, 0.038f, 0.94f);
    const glm::vec4 accent(0.95f, 0.74f, 0.24f, 1.0f);
    const glm::vec4 textMain(0.94f, 0.92f, 0.84f, 1.0f);
    const glm::vec4 textMuted(0.56f, 0.64f, 0.74f, 1.0f);
    const glm::vec4 startButton(0.10f, 0.25f, 0.36f, 0.98f);
    const glm::vec4 exitButton(0.28f, 0.09f, 0.09f, 0.98f);
    const glm::vec4 selectedButton(0.95f, 0.74f, 0.24f, 0.98f);

    AddOverlayRect(vertices, 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), bgTop);
    AddOverlayRect(vertices, 0.0f, height * 0.52f, static_cast<float>(width), height * 0.48f, bgBottom);

    const float panelX = 700.0f;
    const float panelY = 300.0f;
    const float panelW = 520.0f;
    const float panelH = 430.0f;
    AddOverlayRect(vertices, panelX + 12.0f, panelY + 14.0f, panelW, panelH, glm::vec4(0.0f, 0.0f, 0.0f, 0.34f));
    AddOverlayRect(vertices, panelX, panelY, panelW, panelH, panelColor);
    AddOverlayRect(vertices, panelX, panelY, panelW, 6.0f, accent);

    AddOverlayText(vertices, "EXPLORERMAPS", panelX + 64.0f, panelY + 74.0f, 1.85f, accent);
    AddOverlayText(vertices, "MENU DE INICIO", panelX + 66.0f, panelY + 132.0f, 0.82f, textMuted);

    AddOverlayRect(vertices, panelX + 64.0f, panelY + 190.0f, panelW - 128.0f, 58.0f, menu.selection == 0 ? selectedButton : startButton);
    AddOverlayRect(vertices, panelX + 64.0f, panelY + 270.0f, panelW - 128.0f, 58.0f, menu.selection == 1 ? selectedButton : exitButton);
    AddOverlayText(vertices, "INICIAR MUNDO", panelX + 128.0f, panelY + 211.0f, 0.96f, menu.selection == 0 ? glm::vec4(0.04f, 0.05f, 0.07f, 1.0f) : textMain);
    AddOverlayText(vertices, "CERRAR PROGRAMA", panelX + 112.0f, panelY + 291.0f, 0.90f, menu.selection == 1 ? glm::vec4(0.04f, 0.05f, 0.07f, 1.0f) : glm::vec4(1.0f, 0.86f, 0.82f, 1.0f));
    AddOverlayText(vertices, "ExplorerMaps", panelX + 66.0f, panelY + 370.0f, 0.64f, textMuted);

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

void DrawLoadingScreenOverlay(Shader& overlayShader, GLuint overlayVAO, GLuint overlayVBO)
{
    std::vector<OverlayVertex> vertices;
    const float pulse = (std::sin(static_cast<float>(glfwGetTime()) * 5.0f) + 1.0f) * 0.5f;
    const float barW = 360.0f;
    const float progressW = barW * (0.35f + pulse * 0.55f);
    const float centerX = width * 0.5f;
    const float centerY = height * 0.5f;

    AddOverlayRect(vertices, 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), glm::vec4(0.015f, 0.018f, 0.026f, 1.0f));
    AddOverlayText(vertices, "CARGANDO EXPLORERMAN", centerX - 260.0f, centerY - 80.0f, 1.25f, glm::vec4(1.0f, 0.86f, 0.34f, 1.0f));
    AddOverlayRect(vertices, centerX - barW * 0.5f, centerY - 12.0f, barW, 18.0f, glm::vec4(0.08f, 0.10f, 0.14f, 1.0f));
    AddOverlayRect(vertices, centerX - barW * 0.5f, centerY - 12.0f, progressW, 18.0f, glm::vec4(0.14f, 0.40f, 0.56f, 1.0f));
    AddOverlayText(vertices, "PREPARANDO EL MUNDO...", centerX - 170.0f, centerY + 44.0f, 0.72f, glm::vec4(0.64f, 0.71f, 0.82f, 1.0f));

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

MainMenuAction HandleMainMenuInput(GLFWwindow* window, MainMenuState& menu)
{
    const bool mouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool upDown = glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    const bool downDown = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    const bool enterDown = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
    bool gamepadUpDown = false;
    bool gamepadDownDown = false;
    bool gamepadAcceptDown = false;
    bool gamepadCancelDown = false;
    MainMenuAction action = MainMenuAction::None;

    if (glfwJoystickIsGamepad(GLFW_JOYSTICK_1))
    {
        GLFWgamepadstate state;
        if (glfwGetGamepadState(GLFW_JOYSTICK_1, &state))
        {
            const float stickDeadzone = 0.55f;
            gamepadUpDown = state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] == GLFW_PRESS ||
                state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] < -stickDeadzone;
            gamepadDownDown = state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] == GLFW_PRESS ||
                state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] > stickDeadzone;
            gamepadAcceptDown = state.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS;
            gamepadCancelDown = state.buttons[GLFW_GAMEPAD_BUTTON_B] == GLFW_PRESS;
        }
    }

    if ((upDown && !menu.upWasDown) || (gamepadUpDown && !menu.gamepadUpWasDown))
        menu.selection = (menu.selection + 1) % 2;
    if ((downDown && !menu.downWasDown) || (gamepadDownDown && !menu.gamepadDownWasDown))
        menu.selection = (menu.selection + 1) % 2;

    if ((enterDown && !menu.enterWasDown) || (gamepadAcceptDown && !menu.gamepadAcceptWasDown))
        action = menu.selection == 0 ? MainMenuAction::StartWorld : MainMenuAction::Quit;
    if (gamepadCancelDown && !menu.gamepadCancelWasDown)
        action = MainMenuAction::Quit;

    if (mouseDown && !menu.mouseWasDown)
    {
        double mouseX = 0.0;
        double mouseY = 0.0;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        const float buttonX = 764.0f;
        const float buttonW = 392.0f;
        if (IsInsideRect(mouseX, mouseY, buttonX, 490.0f, buttonW, 58.0f))
        {
            menu.selection = 0;
            action = MainMenuAction::StartWorld;
        }
        else if (IsInsideRect(mouseX, mouseY, buttonX, 570.0f, buttonW, 58.0f))
        {
            menu.selection = 1;
            action = MainMenuAction::Quit;
        }
    }

    menu.mouseWasDown = mouseDown;
    menu.upWasDown = upDown;
    menu.downWasDown = downDown;
    menu.enterWasDown = enterDown;
    menu.gamepadUpWasDown = gamepadUpDown;
    menu.gamepadDownWasDown = gamepadDownDown;
    menu.gamepadAcceptWasDown = gamepadAcceptDown;
    menu.gamepadCancelWasDown = gamepadCancelDown;
    return action;
}

void RunLoadingOverlay(GLFWwindow* window, Shader& overlayShader, GLuint overlayVAO, GLuint overlayVBO, float seconds)
{
    const float endTime = static_cast<float>(glfwGetTime()) + seconds;
    while (!glfwWindowShouldClose(window) && static_cast<float>(glfwGetTime()) < endTime)
    {
        glClearColor(0.015f, 0.018f, 0.026f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        DrawLoadingScreenOverlay(overlayShader, overlayVAO, overlayVBO);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
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

MainMenuAction DrawMainMenuImGui()
{
    MainMenuAction action = MainMenuAction::None;
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.025f, 0.035f, 1.0f));
    ImGui::Begin("ExplorerMan Inicio", nullptr, flags);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 size = io.DisplaySize;
    drawList->AddRectFilled(ImVec2(0.0f, 0.0f), size, IM_COL32(8, 13, 20, 255));
    drawList->AddRectFilledMultiColor(
        ImVec2(0.0f, 0.0f),
        size,
        IM_COL32(12, 20, 32, 255),
        IM_COL32(25, 35, 44, 255),
        IM_COL32(5, 8, 14, 255),
        IM_COL32(14, 19, 27, 255));

    const float panelW = 520.0f;
    const float panelH = 430.0f;
    const ImVec2 panelPos((size.x - panelW) * 0.5f, (size.y - panelH) * 0.5f);
    drawList->AddRectFilled(
        ImVec2(panelPos.x + 12.0f, panelPos.y + 14.0f),
        ImVec2(panelPos.x + panelW + 12.0f, panelPos.y + panelH + 14.0f),
        IM_COL32(0, 0, 0, 88),
        6.0f);
    drawList->AddRectFilled(panelPos, ImVec2(panelPos.x + panelW, panelPos.y + panelH), IM_COL32(13, 18, 27, 238), 6.0f);
    drawList->AddRectFilled(panelPos, ImVec2(panelPos.x + panelW, panelPos.y + 6.0f), IM_COL32(238, 188, 63, 255), 6.0f);

    ImGui::SetCursorScreenPos(ImVec2(panelPos.x + 64.0f, panelPos.y + 62.0f));
    ImGui::TextColored(ImVec4(1.0f, 0.86f, 0.34f, 1.0f), "EXPLORERMAN");
    ImGui::SetCursorScreenPos(ImVec2(panelPos.x + 66.0f, panelPos.y + 112.0f));
    ImGui::TextColored(ImVec4(0.66f, 0.73f, 0.82f, 1.0f), "Menu de inicio");

    ImGui::SetCursorScreenPos(ImVec2(panelPos.x + 64.0f, panelPos.y + 180.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(18.0f, 14.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.30f, 0.42f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.40f, 0.56f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.27f, 0.50f, 0.68f, 1.0f));
    if (ImGui::Button("Iniciar mundo", ImVec2(panelW - 128.0f, 58.0f)))
        action = MainMenuAction::StartWorld;
    ImGui::PopStyleColor(3);

    ImGui::SetCursorScreenPos(ImVec2(panelPos.x + 64.0f, panelPos.y + 260.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.34f, 0.12f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.48f, 0.18f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.62f, 0.24f, 0.20f, 1.0f));
    if (ImGui::Button("Cerrar programa", ImVec2(panelW - 128.0f, 58.0f)))
        action = MainMenuAction::Quit;
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);

    ImGui::SetCursorScreenPos(ImVec2(panelPos.x + 66.0f, panelPos.y + 362.0f));
    ImGui::TextColored(ImVec4(0.50f, 0.57f, 0.68f, 1.0f), "ExplorerMan");

    ImGui::End();
    return action;
}

void DrawLoadingScreenImGui()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.015f, 0.018f, 0.026f, 1.0f));
    ImGui::Begin("ExplorerMan Cargando", nullptr, flags);
    ImGui::PopStyleColor();

    const ImVec2 size = io.DisplaySize;
    const float centerX = size.x * 0.5f;
    const float centerY = size.y * 0.5f;
    const float pulse = (std::sin(static_cast<float>(glfwGetTime()) * 5.0f) + 1.0f) * 0.5f;
    const float progress = 0.35f + pulse * 0.55f;

    ImGui::SetCursorScreenPos(ImVec2(centerX - 180.0f, centerY - 64.0f));
    ImGui::TextColored(ImVec4(1.0f, 0.86f, 0.34f, 1.0f), "CARGANDO EXPLORERMAN");
    ImGui::SetCursorScreenPos(ImVec2(centerX - 180.0f, centerY - 10.0f));
    ImGui::ProgressBar(progress, ImVec2(360.0f, 18.0f), "");
    ImGui::SetCursorScreenPos(ImVec2(centerX - 110.0f, centerY + 34.0f));
    ImGui::TextColored(ImVec4(0.64f, 0.71f, 0.82f, 1.0f), "Preparando el mundo...");

    ImGui::End();
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

void HandleCollisionEditorCameraControls(GLFWwindow* window, Camera& camera, float deltaTime)
{
    static bool rightMouseActive = false;
    static double lastMouseX = 0.0;
    static double lastMouseY = 0.0;

    const bool rightMouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    if (!rightMouseDown || ImGui::IsAnyItemActive())
    {
        rightMouseActive = false;
        return;
    }

    double mouseX = 0.0;
    double mouseY = 0.0;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    if (!rightMouseActive)
    {
        rightMouseActive = true;
        lastMouseX = mouseX;
        lastMouseY = mouseY;
    }

    const float mouseDeltaX = static_cast<float>(mouseX - lastMouseX);
    const float mouseDeltaY = static_cast<float>(mouseY - lastMouseY);
    lastMouseX = mouseX;
    lastMouseY = mouseY;

    glm::vec3 forward = glm::length(camera.Orientation) > 0.0001f
        ? glm::normalize(camera.Orientation)
        : glm::vec3(0.0f, 0.0f, -1.0f);

    const float lookScale = camera.sensitivity * 0.0015f;
    glm::vec3 pitchAxis = glm::cross(forward, camera.Up);
    if (glm::length(pitchAxis) < 0.0001f)
    {
        pitchAxis = glm::vec3(1.0f, 0.0f, 0.0f);
    }
    pitchAxis = glm::normalize(pitchAxis);

    const glm::mat4 pitchRotation = glm::rotate(glm::mat4(1.0f), glm::radians(-mouseDeltaY * lookScale), pitchAxis);
    const glm::vec3 pitchedForward = glm::vec3(pitchRotation * glm::vec4(forward, 0.0f));
    const float orientationDot = glm::clamp(glm::dot(glm::normalize(pitchedForward), glm::normalize(camera.Up)), -1.0f, 1.0f);
    const float orientationAngle = std::acos(orientationDot);
    if (std::abs(orientationAngle - glm::radians(90.0f)) <= glm::radians(85.0f))
    {
        forward = glm::normalize(pitchedForward);
    }

    const glm::mat4 yawRotation = glm::rotate(glm::mat4(1.0f), glm::radians(-mouseDeltaX * lookScale), camera.Up);
    camera.Orientation = glm::normalize(glm::vec3(yawRotation * glm::vec4(forward, 0.0f)));

    glm::vec3 moveForward = camera.Orientation;
    if (!camera.flyMode)
    {
        moveForward.y = 0.0f;
    }
    if (glm::length(moveForward) < 0.0001f)
    {
        moveForward = glm::vec3(0.0f, 0.0f, -1.0f);
    }
    moveForward = glm::normalize(moveForward);

    glm::vec3 right = glm::cross(moveForward, camera.Up);
    if (glm::length(right) < 0.0001f)
    {
        right = glm::vec3(1.0f, 0.0f, 0.0f);
    }
    right = glm::normalize(right);

    float currentSpeed = camera.speed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
    {
        currentSpeed *= 1.8f;
    }

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.Position += currentSpeed * moveForward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.Position -= currentSpeed * moveForward;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.Position -= currentSpeed * right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.Position += currentSpeed * right;
    if (camera.flyMode && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.Position += currentSpeed * camera.Up;
    if (camera.flyMode && (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS))
        camera.Position -= currentSpeed * camera.Up;
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

void UploadDirectionalLightUniforms(
    Shader& shaderProgram,
    const glm::vec3& sunDirection,
    const glm::vec3& sunColor,
    float sunIntensity,
    const glm::vec3& moonDirection,
    const glm::vec3& moonColor,
    float moonIntensity,
    const glm::vec3& ambientColor,
    float sunHeight,
    float dayFactor,
    float nightFactor)
{
    glUniform3f(glGetUniformLocation(shaderProgram.ID, "sunDirection"), sunDirection.x, sunDirection.y, sunDirection.z);
    glUniform3f(glGetUniformLocation(shaderProgram.ID, "sunColor"), sunColor.x, sunColor.y, sunColor.z);
    glUniform1f(glGetUniformLocation(shaderProgram.ID, "sunIntensity"), sunIntensity);
    glUniform3f(glGetUniformLocation(shaderProgram.ID, "moonDirection"), moonDirection.x, moonDirection.y, moonDirection.z);
    glUniform3f(glGetUniformLocation(shaderProgram.ID, "moonColor"), moonColor.x, moonColor.y, moonColor.z);
    glUniform1f(glGetUniformLocation(shaderProgram.ID, "moonIntensity"), moonIntensity);
    glUniform3f(glGetUniformLocation(shaderProgram.ID, "ambientColor"), ambientColor.x, ambientColor.y, ambientColor.z);
    glUniform1f(glGetUniformLocation(shaderProgram.ID, "sunHeight"), sunHeight);
    glUniform1f(glGetUniformLocation(shaderProgram.ID, "dayFactor"), dayFactor);
    glUniform1f(glGetUniformLocation(shaderProgram.ID, "nightFactor"), nightFactor);
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

int main(int argc, char** argv)
{
    if (argc > 0)
        SetWorkingDirectoryFromExecutable(argv[0]);

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(width, height, "ExplorerMan", NULL, NULL);
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

    Shader overlayShader("Shaders/menu.vert", "Shaders/menu.frag");
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

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    SetupSkyPanelStyle();
    ImGuiIO& imguiIo = ImGui::GetIO();
    imguiIo.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    LoadApplicationFont(imguiIo);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    MainMenuState mainMenu;
    while (!glfwWindowShouldClose(window))
    {
        const MainMenuAction action = HandleMainMenuInput(window, mainMenu);
        if (action == MainMenuAction::StartWorld)
            break;
        if (action == MainMenuAction::Quit)
            glfwSetWindowShouldClose(window, true);

        glClearColor(0.02f, 0.025f, 0.035f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        DrawMainMenuOverlay(mainMenu, overlayShader, overlayVAO, overlayVBO);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    if (glfwWindowShouldClose(window))
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 0;
    }

    RunLoadingOverlay(window, overlayShader, overlayVAO, overlayVBO, 0.85f);

    Shader shaderProgram("Shaders/default.vert", "Shaders/default.frag");
    Shader lightShader("Shaders/light.vert", "Shaders/light.frag");
    Shader sphereShader("Shaders/sphere.vert", "Shaders/sphere.frag");
    Texture sunTexture("Texturas/sun.jpg", "diffuse", 0);
    Texture moonTexture("Texturas/moon.jpg", "diffuse", 0);
    sphereShader.Activate();
    sunTexture.texUnit(sphereShader, "sphereTexture", 0);

    GLuint sunVAO, sunVBO, sunEBO;
    GLuint moonVAO, moonVBO, moonEBO;
    GLuint lampGlowVAO, lampGlowVBO, lampGlowEBO;
    GLuint lightCubeVAO, lightCubeVBO;
    createSphere(sunVAO, sunVBO, sunEBO, 48, SUN_SIZE);
    createSphere(moonVAO, moonVBO, moonEBO, 36, MOON_SIZE);
    createSphere(lampGlowVAO, lampGlowVBO, lampGlowEBO, lampGlowSectors, lampGlowSize);
    createCube(lightCubeVAO, lightCubeVBO);

    EditorSceneData sceneData;
    Entity cityEntity;
    cityEntity.id = "city_root";
    cityEntity.name = "City";
    cityEntity.assetPath = "modelos/city2.glb";
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

    const std::string activeModelPath = (!sceneData.entities.empty() && !sceneData.entities.front().assetPath.empty())
        ? sceneData.entities.front().assetPath
        : std::string("modelos/city2.glb");
    Model model(activeModelPath.c_str());
    Model carModel("modelos/Coches/Car_1/source/Car1.gltf");
    const glm::vec3 carLocalAnchorOffset = ComputeCarLocalAnchorOffset(carModel);
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
    glm::vec3 sunDirection(0.0f, -1.0f, 0.0f);
    glm::vec3 moonDirection(0.0f, 1.0f, 0.0f);
    glm::vec3 sunLightColor(1.0f);
    float sunLightIntensity = 1.0f;
    glm::vec3 moonLightColor(1.0f);
    float moonLightIntensity = 1.0f;
    ColliderManager colliderManager;
    CollisionSystem collisionSystem;
    DrivableCarState car;

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
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "objectLightBoost"), 0.0f);
        glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), mainLightPos.x, mainLightPos.y, mainLightPos.z);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "moonPos"), moonPos.x, moonPos.y, moonPos.z);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "viewPos"), request.cameraPosition.x, request.cameraPosition.y, request.cameraPosition.z);
        UploadDirectionalLightUniforms(
            shaderProgram,
            sunDirection,
            sunLightColor,
            sunLightIntensity,
            moonDirection,
            moonLightColor,
            moonLightIntensity,
            ambientColor,
            sunHeight,
            dayFactor,
            nightFactor);
        UploadLampLightUniforms(shaderProgram, sceneData.lights);

        for (const Entity& entity : sceneData.entities)
        {
            model.Draw(shaderProgram, previewCamera, BuildSceneModelTransform(entity, baseModelTransform));
        }
        DrawDrivableCar(shaderProgram, previewCamera, carModel, car, carLocalAnchorOffset);

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
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "objectLightBoost"), 0.0f);
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
        DrawDrivableCar(shaderProgram, previewCamera, carModel, car, carLocalAnchorOffset);

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
    InteractionState interaction;
    WalkAnimationState walkAnimation;
    glm::vec3 snappedCarStart;
    if (model.TrySnapToWalkableSurface(
        car.position + glm::vec3(0.0f, 10.0f, 0.0f),
        BuildSceneModelTransform(sceneData.entities.front(), baseModelTransform),
        18.0f,
        10.0f,
        18.0f,
        55.0f,
        walkMaxSlopeDegrees,
        snappedCarStart))
    {
        car.position = snappedCarStart - glm::vec3(0.0f, 10.0f, 0.0f);
        car.groundHeightInitialized = true;
    }
    AppScreen appScreen = AppScreen::World;
    LoadingDestination loadingDestination = LoadingDestination::World;
    float loadingScreenUntil = 0.0f;
    bool menuCursorVisible = false;
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        if (appScreen == AppScreen::MainMenu)
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            menuCursorVisible = true;
            camera.firstClick = true;
            glfwSetWindowTitle(window, "ExplorerMan");

            const MainMenuAction action = HandleMainMenuInput(window, mainMenu);
            if (action == MainMenuAction::StartWorld)
            {
                appScreen = AppScreen::Loading;
                loadingDestination = LoadingDestination::World;
                loadingScreenUntil = currentFrame + 0.85f;
                mainMenu.mouseWasDown = true;
            }
            else if (action == MainMenuAction::Quit)
            {
                glfwSetWindowShouldClose(window, true);
            }

            glClearColor(0.02f, 0.025f, 0.035f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            DrawMainMenuOverlay(mainMenu, overlayShader, overlayVAO, overlayVBO);
            glfwSwapBuffers(window);
            glfwPollEvents();
            continue;
        }

        if (appScreen == AppScreen::Loading)
        {
            if (currentFrame >= loadingScreenUntil)
            {
                if (loadingDestination == LoadingDestination::World)
                {
                    appScreen = AppScreen::World;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    menuCursorVisible = false;
                    camera.firstClick = true;
                }
                else
                {
                    appScreen = AppScreen::MainMenu;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    menuCursorVisible = true;
                    camera.firstClick = true;
                    mainMenu.mouseWasDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
                }
            }

            glClearColor(0.015f, 0.018f, 0.026f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            DrawLoadingScreenOverlay(overlayShader, overlayVAO, overlayVBO);
            glfwSwapBuffers(window);
            glfwPollEvents();
            continue;
        }

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
            ImGui::Render();
            appScreen = AppScreen::Loading;
            loadingDestination = LoadingDestination::MainMenu;
            loadingScreenUntil = currentFrame + 0.85f;
            environmentMenu.open = false;
            skyPanel.open = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            menuCursorVisible = true;
            camera.firstClick = true;
            continue;
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

        const float dayLightBlend = glm::smoothstep(-0.26f, 0.34f, sunHeight);
        const float sunWarmBlend = glm::smoothstep(-0.10f, 0.28f, sunHeight);
        const float sunNoonBlend = glm::smoothstep(0.22f, 0.82f, sunHeight);
        const float sunPresence = glm::smoothstep(-0.18f, 0.78f, sunHeight);
        const float moonPresence = glm::smoothstep(0.20f, -0.80f, sunHeight);
        const float moonHeight = glm::clamp(std::abs(moonVertical), 0.0f, 1.0f);
        sunDirection = glm::normalize(-sunPos);
        moonDirection = glm::normalize(-moonPos);

        const glm::vec3 sunWarmColor = glm::mix(
            glm::vec3(0.92f, 0.46f, 0.28f),
            glm::vec3(1.0f, 0.76f, 0.56f),
            sunWarmBlend
        );
        sunLightColor = glm::mix(
            sunWarmColor,
            glm::vec3(1.0f, 0.98f, 0.92f),
            sunNoonBlend
        );
        sunLightIntensity = glm::mix(0.08f, 1.18f, sunPresence);
        const glm::vec3 sunAmbientColor = glm::mix(
            glm::vec3(0.08f, 0.07f, 0.10f),
            glm::vec3(0.35f, 0.38f, 0.42f),
            glm::smoothstep(-0.02f, 0.72f, sunHeight)
        );
        const float sunDiffuseIntensity = glm::mix(0.12f, 1.0f, sunPresence);
        const float sunSpecularIntensity = glm::mix(0.08f, 0.60f, sunPresence);

        moonLightColor = glm::mix(
            glm::vec3(0.50f, 0.56f, 0.74f),
            glm::vec3(0.65f, 0.70f, 0.90f),
            moonHeight
        );
        moonLightIntensity = glm::mix(0.18f, 0.34f, moonPresence * moonHeight);
        const glm::vec3 moonAmbientColor = glm::mix(
            glm::vec3(0.05f, 0.06f, 0.08f),
            glm::vec3(0.08f, 0.10f, 0.15f),
            glm::smoothstep(-0.90f, -0.10f, sunHeight)
        );
        const float moonDiffuseIntensity = glm::mix(0.18f, 0.35f, moonPresence);
        const float moonSpecularIntensity = glm::mix(0.06f, 0.15f, moonPresence);

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
        const glm::vec3 twilightSkyColor(0.72f, 0.44f, 0.42f);
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
        const GameplayGamepadInput gamepad = ReadGameplayGamepadInput();
        bool playerWalking = false;
        if (!environmentMenu.open && !skyPanel.open && !editor.IsActive() && !collisionEditor.IsActive())
        {
            playerWalking = !car.driving && !camera.flyMode && IsGameplayMovementPressed(window, gamepad);
            if (environmentMode == EnvironmentMode::Manual && !skyPanel.open)
            {
                const bool decreaseTime = glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_4) == GLFW_PRESS;
                const bool increaseTime = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_6) == GLFW_PRESS;
                if (decreaseTime)
                    manualTimeOfDay = WrapTimeOfDay(manualTimeOfDay - deltaTime * 0.08f);
                if (increaseTime)
                    manualTimeOfDay = WrapTimeOfDay(manualTimeOfDay + deltaTime * 0.08f);
            }

            if (!car.driving)
                camera.Inputs(window, deltaTime);

            UpdateDrivableCar(window, camera, car, model, sceneModelTransform, interaction, gamepad, currentFrame, deltaTime);
        }
        else if (!environmentMenu.open && !skyPanel.open && !editor.IsActive() && collisionEditor.IsActive())
        {
            HandleCollisionEditorCameraControls(window, camera, deltaTime);
        }

<<<<<<< Updated upstream
        if (!camera.flyMode && !car.driving)
=======
        const bool editingModeActive = editor.IsActive() || collisionEditor.IsActive();

        if (!camera.flyMode && !editingModeActive)
>>>>>>> Stashed changes
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

        }
        else if (!camera.flyMode && editingModeActive)
        {
            // Editing mode can move outside the map bounds to place colliders.
        }

        if (!car.driving)
        {
            camera.Position = collisionSystem.ResolveCameraPosition(
                colliderManager.GetColliders(),
                prevPos,
                camera.Position,
                cameraCollisionRadius);
        }

        const bool gameplayHudActive = !environmentMenu.open && !skyPanel.open && !editor.IsActive() && !collisionEditor.IsActive();
        UpdateWalkAnimation(walkAnimation, gameplayHudActive && !camera.flyMode && !car.driving && playerWalking, deltaTime);
        interaction.target = SceneSelection();
        interaction.targetName.clear();
        if (!car.driving)
            interaction.interactWasDown = false;

        Camera renderCamera = camera;
        renderCamera.Position += ComputeWalkBobOffset(camera, walkAnimation);
        renderCamera.updateMatrix(cameraFov, cameraNearPlane, cameraFarPlane);
        glm::mat4 view = renderCamera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(cameraFov), (float)width / height, cameraNearPlane, cameraFarPlane);

        shaderProgram.Activate();
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "objectLightBoost"), 0.0f);
        glUniform4f(glGetUniformLocation(shaderProgram.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "lightPos"), mainLightPos.x, mainLightPos.y, mainLightPos.z);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "moonPos"), moonPos.x, moonPos.y, moonPos.z);
<<<<<<< Updated upstream
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "viewPos"), renderCamera.Position.x, renderCamera.Position.y, renderCamera.Position.z);
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "ambientColor"), ambientColor.x, ambientColor.y, ambientColor.z);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "time"), currentFrame);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "dayFactor"), dayFactor);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "nightFactor"), nightFactor);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "isDay"), isDay ? 1.0f : 0.0f);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "diffuseIntensity"), diffuseIntensity);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "specularIntensity"), specularIntensity);
        glUniform1f(glGetUniformLocation(shaderProgram.ID, "sunHeight"), sunHeight);
=======
        glUniform3f(glGetUniformLocation(shaderProgram.ID, "viewPos"), camera.Position.x, camera.Position.y, camera.Position.z);
        UploadDirectionalLightUniforms(
            shaderProgram,
            sunDirection,
            sunLightColor,
            sunLightIntensity,
            moonDirection,
            moonLightColor,
            moonLightIntensity,
            ambientColor,
            sunHeight,
            dayFactor,
            nightFactor);
>>>>>>> Stashed changes
        UploadLampLightUniforms(shaderProgram, sceneData.lights);

        for (const Entity& entity : sceneData.entities)
        {
            model.Draw(shaderProgram, renderCamera, BuildSceneModelTransform(entity, baseModelTransform));
        }
        DrawDrivableCar(shaderProgram, renderCamera, carModel, car, carLocalAnchorOffset);

        DrawInteriorLightCubes(sceneData.lights, lightShader, renderCamera, lightCubeVAO, skySunDirection);

        if (nightFactor > 0.02f) {
            sphereShader.Activate();
            glUniform1f(glGetUniformLocation(sphereShader.ID, "useTexture"), 0.0f);
            glUniform1f(glGetUniformLocation(sphereShader.ID, "unlit"), 1.0f);
            glUniform3f(glGetUniformLocation(sphereShader.ID, "lightPos"), renderCamera.Position.x, renderCamera.Position.y, renderCamera.Position.z);
            glUniform3f(glGetUniformLocation(sphereShader.ID, "viewPos"), renderCamera.Position.x, renderCamera.Position.y, renderCamera.Position.z);
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
            glUniform3f(glGetUniformLocation(sphereShader.ID, "viewPos"), renderCamera.Position.x, renderCamera.Position.y, renderCamera.Position.z);
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
            glUniform3f(glGetUniformLocation(sphereShader.ID, "viewPos"), renderCamera.Position.x, renderCamera.Position.y, renderCamera.Position.z);
            glUniform1f(glGetUniformLocation(sphereShader.ID, "isDay"), 0.0f);
            glUniform1f(glGetUniformLocation(sphereShader.ID, "sunHeight"), sunHeight);

            glBindVertexArray(moonVAO);
            glDrawElements(GL_TRIANGLES, 36 * 36 * 6, GL_UNSIGNED_INT, 0);
            moonTexture.Unbind();
        }

        if (!useFastRenderMode)
            skybox.Draw(renderCamera, cameraFov, cameraNearPlane, cameraFarPlane, currentFrame, sunHeight, skySunDirection);

        if (gameplayHudActive)
            DrawPlayerHud(interaction, walkAnimation, currentFrame, overlayShader, overlayVAO, overlayVBO);
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
