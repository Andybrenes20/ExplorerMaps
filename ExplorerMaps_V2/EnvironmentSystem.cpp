#include "EnvironmentSystem.h"

#include <algorithm>
#include <cmath>

#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>

#include "GameplayInput.h"
#include "imgui/imgui.h"

namespace {
    constexpr float kCelestialOrbitRadius = 950.0f;
    constexpr float kMaxSunHeight = 780.0f;
    constexpr float kMaxMoonHeightFactor = 0.72f;

    float WrapHours(float hours) {
        float wrapped = std::fmod(hours, 24.0f);
        return wrapped < 0.0f ? wrapped + 24.0f : wrapped;
    }

    float HoursToSunAngle(float hours) {
        return ((WrapHours(hours) - 6.0f) / 24.0f) * glm::two_pi<float>();
    }

    float ComputeSkyBlendFactor(float sunHeight) {
        return glm::smoothstep(0.28f, -0.18f, sunHeight);
    }

    float FractFloat(float value) {
        return value - std::floor(value);
    }

    float LightningNoise(float seed) {
        return FractFloat(std::sin(seed * 12.9898f + 78.233f) * 43758.5453f);
    }

    const char* ModeName(EnvironmentMode mode) {
        switch (mode) {
        case EnvironmentMode::Day: return "Dia";
        case EnvironmentMode::Night: return "Noche";
        case EnvironmentMode::Manual: return "Manual";
        default: return "Auto";
        }
    }

    EnvironmentMode ModeFromSelection(int selection) {
        switch (selection) {
        case 0: return EnvironmentMode::Day;
        case 1: return EnvironmentMode::Night;
        case 2: return EnvironmentMode::Auto;
        default: return EnvironmentMode::Manual;
        }
    }

    int SelectionFromMode(EnvironmentMode mode) {
        switch (mode) {
        case EnvironmentMode::Day: return 0;
        case EnvironmentMode::Night: return 1;
        case EnvironmentMode::Auto: return 2;
        case EnvironmentMode::Manual: return 3;
        }
        return 2;
    }
}

EnvironmentFrame EnvironmentSystem::Update(GLFWwindow* window, const GameplayGamepadInput& gamepad, float deltaTime, float currentFrame) {
    // Aqui se actualiza el estado de ambiente: dia/noche/auto/manual, lluvia y rayos.
    HandleControls(window, gamepad);

    if (mode == EnvironmentMode::Day) {
        timeOfDayHours = 12.0f;
    }
    else if (mode == EnvironmentMode::Night) {
        timeOfDayHours = 0.0f;
    }
    else if (mode == EnvironmentMode::Auto) {
        timeOfDayHours = WrapHours(timeOfDayHours + deltaTime * timeScale);
    }

    rainSmoothed += (rainTarget - rainSmoothed) * std::clamp(deltaTime * 2.8f, 0.0f, 1.0f);
    UpdateLightning(currentFrame);

    return BuildFrame(currentFrame);
}

EnvironmentFrame EnvironmentSystem::BuildFrame(float currentFrame) const {
    // Aqui se convierten hora/lluvia/rayos en valores finales para shaders y cielo.
    float sunAngle = HoursToSunAngle(timeOfDayHours);
    float sunHeight = std::sin(sunAngle);

    if (mode == EnvironmentMode::Day) {
        sunAngle = 0.58f;
        sunHeight = std::sin(sunAngle);
    }
    else if (mode == EnvironmentMode::Night) {
        sunAngle = 4.71239f;
        sunHeight = -1.0f;
    }

    const float rawLightningAmount = GetLightningAmount(currentFrame);
    const float lightningRainFactor = glm::smoothstep(0.05f, 0.35f, std::max(rainSmoothed, rainTarget));
    const float lightningAmount = rawLightningAmount * lightningRainFactor;
    const float skyBlendFactor = (mode == EnvironmentMode::Day)
        ? 0.0f
        : (mode == EnvironmentMode::Night ? 1.0f : ComputeSkyBlendFactor(sunHeight));

    const float sunHorizontal = std::cos(sunAngle);
    const float sunVertical = sunHeight;
    glm::vec3 sunPosition(
        sunHorizontal * kCelestialOrbitRadius,
        sunVertical > 0.0f ? sunVertical * kMaxSunHeight : -500.0f,
        std::sin(sunAngle) * kCelestialOrbitRadius * 0.5f);

    const float moonAngle = sunAngle + glm::pi<float>();
    const float moonHorizontal = std::cos(moonAngle);
    const float moonVertical = std::sin(moonAngle);
    glm::vec3 moonPosition(
        moonHorizontal * kCelestialOrbitRadius * 0.8f,
        moonVertical > 0.0f ? moonVertical * kMaxSunHeight * kMaxMoonHeightFactor : -300.0f,
        std::sin(moonAngle) * kCelestialOrbitRadius * 0.4f);

    const float dayLightBlend = glm::smoothstep(-0.14f, 0.18f, sunHeight);
    const float sunWarmBlend = glm::smoothstep(-0.04f, 0.32f, sunHeight);
    const float sunNoonBlend = glm::smoothstep(0.18f, 0.82f, sunHeight);
    const float sunPresence = glm::smoothstep(-0.08f, 0.75f, sunHeight);
    const float moonPresence = glm::smoothstep(0.08f, -0.88f, sunHeight);
    const float moonHeight = glm::clamp(std::abs(moonVertical), 0.0f, 1.0f);

    const glm::vec3 sunWarmColor = glm::mix(
        glm::vec3(1.0f, 0.47f, 0.18f),
        glm::vec3(1.0f, 0.76f, 0.50f),
        sunWarmBlend);
    const glm::vec3 sunLightColor = glm::mix(
        sunWarmColor,
        glm::vec3(1.0f, 0.95f, 0.82f),
        sunNoonBlend);
    const float sunLightIntensity = glm::mix(0.34f, 1.36f, sunPresence);
    const glm::vec3 sunAmbientColor = glm::mix(
        glm::vec3(0.07f, 0.07f, 0.10f),
        glm::vec3(0.25f, 0.29f, 0.34f),
        glm::smoothstep(-0.02f, 0.72f, sunHeight));
    const float sunDiffuseIntensity = glm::mix(0.24f, 1.16f, sunPresence);
    const float sunSpecularIntensity = glm::mix(0.12f, 0.60f, sunPresence);

    const glm::vec3 moonLightColor = glm::mix(
        glm::vec3(0.52f, 0.57f, 0.70f),
        glm::vec3(0.63f, 0.69f, 0.84f),
        moonHeight);
    const float moonLightIntensity = glm::mix(0.18f, 0.38f, moonPresence * moonHeight);
    const glm::vec3 moonAmbientColor = glm::mix(
        glm::vec3(0.045f, 0.052f, 0.070f),
        glm::vec3(0.075f, 0.090f, 0.125f),
        glm::smoothstep(-0.90f, -0.10f, sunHeight));
    const float moonDiffuseIntensity = glm::mix(0.25f, 0.40f, moonPresence);
    const float moonSpecularIntensity = glm::mix(0.13f, 0.20f, moonPresence);

    glm::vec3 mainLightPosition = glm::mix(moonPosition, sunPosition, dayLightBlend);
    glm::vec3 mainLightColor = glm::mix(moonLightColor, sunLightColor, dayLightBlend);
    float mainLightIntensity = glm::mix(moonLightIntensity, sunLightIntensity, dayLightBlend);
    glm::vec3 ambient = glm::mix(moonAmbientColor, sunAmbientColor, dayLightBlend);
    float diffuseIntensity = glm::mix(moonDiffuseIntensity, sunDiffuseIntensity, dayLightBlend);
    float specularIntensity = glm::mix(moonSpecularIntensity, sunSpecularIntensity, dayLightBlend);

    mainLightColor = glm::mix(mainLightColor, glm::vec3(0.58f, 0.66f, 0.78f), rainSmoothed * 0.45f);
    mainLightIntensity *= glm::mix(1.0f, 0.48f, rainSmoothed);
    ambient = glm::mix(ambient, glm::vec3(0.15f, 0.18f, 0.22f), rainSmoothed * 0.70f);
    diffuseIntensity *= glm::mix(1.0f, 0.46f, rainSmoothed);
    specularIntensity = glm::mix(specularIntensity, std::max(specularIntensity, 1.05f), rainSmoothed);

    if (lightningAmount > 0.001f) {
        const float lightningClamp = glm::clamp(lightningAmount, 0.0f, 1.0f);
        mainLightColor = glm::mix(mainLightColor, glm::vec3(0.82f, 0.90f, 1.0f), lightningClamp);
        mainLightIntensity += lightningAmount * 1.75f;
        ambient += glm::vec3(0.28f, 0.34f, 0.46f) * lightningAmount;
        diffuseIntensity = std::max(diffuseIntensity, 0.72f + lightningAmount * 0.45f);
        specularIntensity = std::max(specularIntensity, 1.10f + lightningAmount * 0.45f);
    }

    const glm::vec3 diffuse = mainLightColor * diffuseIntensity * mainLightIntensity;
    const glm::vec3 specular = mainLightColor * specularIntensity * mainLightIntensity;

    glm::vec3 clearColor = glm::mix(
        glm::mix(glm::vec3(0.03f, 0.04f, 0.08f), glm::vec3(0.96f, 0.62f, 0.34f), glm::smoothstep(-0.22f, 0.18f, sunHeight)),
        glm::vec3(0.43f, 0.64f, 0.91f),
        glm::smoothstep(0.08f, 0.72f, sunHeight));
    clearColor = glm::mix(clearColor, glm::vec3(0.20f, 0.24f, 0.30f), rainSmoothed * 0.68f);
    clearColor = glm::mix(clearColor, glm::vec3(0.70f, 0.78f, 0.92f), glm::clamp(lightningAmount * 0.72f, 0.0f, 1.0f));
    clearColor += glm::vec3(0.20f, 0.24f, 0.34f) * lightningAmount;

    EnvironmentFrame frame;
    frame.timeOfDayHours = timeOfDayHours;
    frame.skyBlendFactor = skyBlendFactor;
    frame.blendFactor = std::clamp(skyBlendFactor + rainSmoothed * 0.18f - lightningAmount * 0.12f, 0.0f, 1.0f);
    frame.sunHeight = sunHeight;
    frame.dayFactor = glm::clamp(sunHeight + 0.2f, 0.05f, 1.0f);
    frame.nightFactor = glm::clamp(-sunHeight + 0.1f, 0.0f, 1.0f);
    frame.rainIntensity = rainSmoothed;
    frame.lightningAmount = lightningAmount;
    frame.lightningSeed = lightningSeed;
    frame.sunDirection = glm::normalize(glm::vec3(std::cos(sunAngle), std::sin(sunAngle), std::sin(sunAngle) * 0.5f));
    frame.moonPosition = moonPosition;
    frame.mainLightPosition = mainLightPosition;
    frame.mainLightColor = mainLightColor;
    frame.mainLightIntensity = mainLightIntensity;
    frame.activeLightDir = glm::length(mainLightPosition) > 0.001f ? -glm::normalize(mainLightPosition) : glm::vec3(0.0f, -1.0f, -0.2f);
    frame.ambient = ambient;
    frame.diffuse = diffuse;
    frame.specular = specular;
    frame.streetlightIntensity = std::clamp(frame.nightFactor + rainSmoothed * 0.35f, 0.0f, 1.0f);
    frame.windowLightIntensity = std::clamp(glm::smoothstep(0.08f, 0.62f, frame.nightFactor) + rainSmoothed * 0.18f, 0.0f, 1.0f);
    frame.cloudCoverage = glm::mix(0.72f, 0.90f, rainSmoothed);
    frame.cloudSpeed = glm::mix(0.58f, 1.25f, rainSmoothed);
    frame.cloudDensity = glm::mix(1.08f, 1.70f, rainSmoothed);
    frame.cloudCrispiness = glm::mix(0.84f, 0.58f, rainSmoothed);
    frame.cloudColor = glm::mix(glm::vec3(0.56f, 0.60f, 0.66f), glm::vec3(0.34f, 0.39f, 0.46f), rainSmoothed);
    frame.clearColor = glm::clamp(clearColor, glm::vec3(0.0f), glm::vec3(1.0f));
    return frame;
}

void EnvironmentSystem::DrawMenu() {
    if (!menuOpen) {
        return;
    }

    // Aqui se dibuja el menu de ambiente, controlable por teclado y mando.
    ImGui::SetNextWindowPos(ImVec2(18.0f, 18.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(275.0f, 205.0f), ImGuiCond_Always);
    ImGui::Begin("Ambiente", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    ImGui::Text("Modo: %s", ModeName(mode));
    ImGui::Separator();

    const char* labels[] = { "Dia", "Noche", "Auto", "Manual" };
    for (int i = 0; i < 4; ++i) {
        if (menuSelection == i) {
            ImGui::TextColored(ImVec4(0.95f, 0.72f, 0.24f, 1.0f), "> %s", labels[i]);
        }
        else {
            ImGui::Text("  %s", labels[i]);
        }
    }

    ImGui::Separator();
    ImGui::Text("Hora: %.1f", timeOfDayHours);
    ImGui::SliderFloat("Hora manual", &timeOfDayHours, 0.0f, 24.0f, "%.1f h");
    ImGui::SliderFloat("Velocidad", &timeScale, 0.0f, 5.0f, "%.1fx");
    ImGui::SliderFloat("Lluvia", &rainTarget, 0.0f, 1.0f, "%.2f");
    ImGui::Text("Lluvia: %.0f%%", rainTarget * 100.0f);
    ImGui::End();
}

void EnvironmentSystem::DrawPauseControls() {
    int selectedMode = SelectionFromMode(mode);
    if (ImGui::RadioButton("Dia", &selectedMode, 0)) {
        mode = EnvironmentMode::Day;
    }
    if (ImGui::RadioButton("Noche", &selectedMode, 1)) {
        mode = EnvironmentMode::Night;
    }
    if (ImGui::RadioButton("Auto", &selectedMode, 2)) {
        mode = EnvironmentMode::Auto;
    }
    if (ImGui::RadioButton("Manual", &selectedMode, 3)) {
        mode = EnvironmentMode::Manual;
    }

    ImGui::Separator();
    if (mode == EnvironmentMode::Auto) {
        ImGui::SliderFloat("Velocidad del Tiempo", &timeScale, 0.0f, 5.0f, "%.1fx");
    }
    if (mode == EnvironmentMode::Manual) {
        ImGui::SliderFloat("Ajustar Hora", &timeOfDayHours, 0.0f, 24.0f, "%.1f hrs");
    }
    ImGui::SliderFloat("Lluvia", &rainTarget, 0.0f, 1.0f, "%.2f");
    ImGui::Text("Hora actual: %.2f hrs", timeOfDayHours);
}

bool EnvironmentSystem::IsMenuOpen() const {
    return menuOpen;
}

bool EnvironmentSystem::ConsumeLightningEvent() {
    const bool triggered = lightningTriggered;
    lightningTriggered = false;
    return triggered;
}

void EnvironmentSystem::HandleControls(GLFWwindow* window, const GameplayGamepadInput& gamepad) {
    const bool f3Down = glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS;
    const bool rDown = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    const bool keyUpDown = glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS;
    const bool keyDownDown = glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS;
    const bool keyAcceptDown = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
    const bool keyCancelDown = glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS;

    bool startDown = false;
    bool dpadUpDown = false;
    bool dpadDownDown = false;
    bool acceptDown = false;
    bool cancelDown = false;

    if (gamepad.connected) {
        GLFWgamepadstate state;
        if (glfwGetGamepadState(GLFW_JOYSTICK_1, &state)) {
            startDown = state.buttons[GLFW_GAMEPAD_BUTTON_START] == GLFW_PRESS;
            dpadUpDown = state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] == GLFW_PRESS || gamepad.leftY < -0.55f;
            dpadDownDown = state.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] == GLFW_PRESS || gamepad.leftY > 0.55f;
            acceptDown = state.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS;
            cancelDown = state.buttons[GLFW_GAMEPAD_BUTTON_B] == GLFW_PRESS;
        }
    }

    if ((f3Down && !f3WasDown) || (startDown && !startWasDown)) {
        menuOpen = !menuOpen;
        menuSelection = SelectionFromMode(mode);
    }

    if (rDown && !rWasDown && !menuOpen) {
        rainTarget = rainTarget <= 0.05f ? 0.78f : 0.0f;
        if (rainTarget > 0.05f) {
            nextLightningTime = 0.0f;
        }
    }

    if (menuOpen) {
        const bool upDown = dpadUpDown || keyUpDown;
        const bool downDown = dpadDownDown || keyDownDown;
        const bool acceptPressed = acceptDown || keyAcceptDown;
        const bool cancelPressed = cancelDown || keyCancelDown;

        if (upDown && !upWasDown) {
            menuSelection = (menuSelection + 3) % 4;
        }
        if (downDown && !downWasDown) {
            menuSelection = (menuSelection + 1) % 4;
        }
        if (acceptPressed && !acceptWasDown) {
            mode = ModeFromSelection(menuSelection);
            menuOpen = mode == EnvironmentMode::Manual;
        }
        if (cancelPressed && !cancelWasDown) {
            menuOpen = false;
        }

        if (mode == EnvironmentMode::Manual) {
            if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
                timeOfDayHours = WrapHours(timeOfDayHours - 0.08f);
            }
            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
                timeOfDayHours = WrapHours(timeOfDayHours + 0.08f);
            }
        }
    }

    f3WasDown = f3Down;
    rWasDown = rDown;
    startWasDown = startDown;
    upWasDown = dpadUpDown || keyUpDown;
    downWasDown = dpadDownDown || keyDownDown;
    acceptWasDown = acceptDown || keyAcceptDown;
    cancelWasDown = cancelDown || keyCancelDown;
}

void EnvironmentSystem::UpdateLightning(float currentFrame) {
    if (rainSmoothed <= 0.08f) {
        nextLightningTime = currentFrame + 1.2f;
        return;
    }

    if (nextLightningTime <= 0.01f) {
        nextLightningTime = currentFrame + 3.0f + LightningNoise(currentFrame + 9.0f) * 4.0f;
    }

    if (currentFrame >= nextLightningTime) {
        lightningStart = currentFrame;
        lightningDuration = 0.28f + LightningNoise(currentFrame + 3.7f) * 0.20f;
        lightningStrength = 0.56f + rainSmoothed * 0.36f;
        lightningSeed = LightningNoise(currentFrame + lightningStrength * 17.0f);
        nextLightningTime = currentFrame + 5.5f + LightningNoise(currentFrame + 21.0f) * 7.5f;
        lightningTriggered = true;
    }
}

float EnvironmentSystem::GetLightningAmount(float currentFrame) const {
    if (lightningDuration <= 0.0f) {
        return 0.0f;
    }

    const float t = (currentFrame - lightningStart) / lightningDuration;
    if (t < 0.0f || t > 1.0f) {
        return 0.0f;
    }

    const float firstPulse = 1.0f - glm::smoothstep(0.0f, 0.18f, t);
    const float secondPulse = glm::smoothstep(0.16f, 0.24f, t) *
        (1.0f - glm::smoothstep(0.24f, 0.52f, t)) * 0.78f;
    const float afterGlow = (1.0f - glm::smoothstep(0.48f, 1.0f, t)) * 0.20f;
    const float flicker = 0.86f + 0.14f * std::sin((t + lightningSeed) * 72.0f);
    return std::clamp((firstPulse + secondPulse + afterGlow) * lightningStrength * flicker, 0.0f, 1.35f);
}
