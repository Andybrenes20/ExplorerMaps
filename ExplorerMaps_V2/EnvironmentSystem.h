#pragma once

#include <glm/glm.hpp>

struct GLFWwindow;
struct GameplayGamepadInput;

enum class EnvironmentMode {
    Auto,
    Day,
    Night,
    Manual
};

struct EnvironmentFrame {
    float timeOfDayHours = 12.0f;
    float blendFactor = 0.0f;
    float skyBlendFactor = 0.0f;
    float sunHeight = 1.0f;
    float dayFactor = 1.0f;
    float nightFactor = 0.0f;
    float rainIntensity = 0.0f;
    float lightningAmount = 0.0f;
    float lightningSeed = 0.0f;
    glm::vec3 sunDirection = glm::vec3(0.0f, 1.0f, 0.2f);
    glm::vec3 moonPosition = glm::vec3(0.0f);
    glm::vec3 mainLightPosition = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 mainLightColor = glm::vec3(1.0f);
    float mainLightIntensity = 1.0f;
    glm::vec3 activeLightDir = glm::vec3(0.0f, -1.0f, -0.2f);
    glm::vec3 ambient = glm::vec3(0.4f);
    glm::vec3 diffuse = glm::vec3(0.9f, 0.8f, 0.7f);
    glm::vec3 specular = glm::vec3(1.0f);
    float streetlightIntensity = 0.0f;
    float windowLightIntensity = 0.0f;
    float cloudCoverage = 0.72f;
    float cloudSpeed = 0.65f;
    float cloudDensity = 1.08f;
    float cloudCrispiness = 0.82f;
    glm::vec3 cloudColor = glm::vec3(0.56f, 0.60f, 0.66f);
    glm::vec3 clearColor = glm::vec3(0.4f);
};

// Funcionalidad: cambios de ambiente, lluvia/rayos y menu de ambiente.
// Implementacion completa en EnvironmentSystem.cpp.
class EnvironmentSystem {
public:
    EnvironmentFrame Update(GLFWwindow* window, const GameplayGamepadInput& gamepad, float deltaTime, float currentFrame);
    EnvironmentFrame BuildFrame(float currentFrame) const;
    void DrawMenu();
    void DrawPauseControls();
    bool IsMenuOpen() const;
    bool ConsumeLightningEvent();

private:
    EnvironmentMode mode = EnvironmentMode::Auto;
    float timeOfDayHours = 10.0f;
    float timeScale = 0.5f;
    float rainTarget = 0.0f;
    float rainSmoothed = 0.0f;
    float lightningStart = -10.0f;
    float lightningDuration = 0.0f;
    float lightningStrength = 0.0f;
    float lightningSeed = 0.0f;
    float nextLightningTime = 0.0f;
    bool lightningTriggered = false;
    bool menuOpen = false;
    int menuSelection = 0;
    bool f3WasDown = false;
    bool rWasDown = false;
    bool startWasDown = false;
    bool upWasDown = false;
    bool downWasDown = false;
    bool acceptWasDown = false;
    bool cancelWasDown = false;

    void HandleControls(GLFWwindow* window, const GameplayGamepadInput& gamepad);
    void UpdateLightning(float currentFrame);
    float GetLightningAmount(float currentFrame) const;
};
