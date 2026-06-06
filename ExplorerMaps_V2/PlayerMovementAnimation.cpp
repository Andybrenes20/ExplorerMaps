#include "PlayerMovementAnimation.h"

#include <algorithm>
#include <cmath>

#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>

#include "GameplayInput.h"

namespace {
    float Clamp01(float value) {
        return std::clamp(value, 0.0f, 1.0f);
    }
}

void PlayerMovementAnimation::Update(GLFWwindow* window, const GameplayGamepadInput& gamepad, float deltaTime, const glm::vec3& cameraFront, const glm::vec3& cameraUp, bool enabled) {
    // Aqui se calcula el balanceo de camara para caminar y correr.
    moving = enabled && GameplayInput::IsMovementPressed(window, gamepad);
    running = moving && GameplayInput::IsRunning(window, gamepad);

    const float targetBlend = moving ? 1.0f : 0.0f;
    blend += (targetBlend - blend) * Clamp01(deltaTime * 9.0f);

    const float frequency = running ? 13.0f : 8.0f;
    const float verticalAmplitude = running ? 0.095f : 0.055f;
    const float sideAmplitude = running ? 0.045f : 0.025f;

    phase += deltaTime * frequency;
    if (phase > glm::two_pi<float>()) {
        phase = std::fmod(phase, glm::two_pi<float>());
    }

    glm::vec3 forwardFlat(cameraFront.x, 0.0f, cameraFront.z);
    if (glm::length(forwardFlat) <= 0.0001f) {
        forwardFlat = glm::vec3(0.0f, 0.0f, -1.0f);
    }
    forwardFlat = glm::normalize(forwardFlat);

    glm::vec3 right = glm::cross(forwardFlat, cameraUp);
    if (glm::length(right) <= 0.0001f) {
        right = glm::vec3(1.0f, 0.0f, 0.0f);
    }
    else {
        right = glm::normalize(right);
    }

    const float verticalBob = std::sin(phase * 2.0f) * verticalAmplitude * blend;
    const float sideBob = std::sin(phase) * sideAmplitude * blend;
    cameraOffset = cameraUp * verticalBob + right * sideBob;
}

glm::vec3 PlayerMovementAnimation::GetCameraOffset() const {
    return cameraOffset;
}

bool PlayerMovementAnimation::IsMoving() const {
    return moving;
}

bool PlayerMovementAnimation::IsRunning() const {
    return running;
}
