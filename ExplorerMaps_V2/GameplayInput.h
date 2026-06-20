#pragma once

#include <glm/glm.hpp>

struct GLFWwindow;

struct GameplayGamepadInput {
    bool connected = false;
    float leftX = 0.0f;
    float leftY = 0.0f;
    float rightX = 0.0f;
    float rightY = 0.0f;
    float leftTrigger = 0.0f;
    float rightTrigger = 0.0f;
    bool interactDown = false;
    bool pauseDown = false;
    bool handbrakeDown = false;
};

// Funcionalidad: lectura del mando y mezcla con teclado/mouse.
// Implementacion completa en GameplayInput.cpp.
namespace GameplayInput {
    GameplayGamepadInput ReadGamepad();
    bool IsRunning(GLFWwindow* window, const GameplayGamepadInput& gamepad);
    bool IsMovementPressed(GLFWwindow* window, const GameplayGamepadInput& gamepad);
    glm::vec3 BuildMoveDirection(
        GLFWwindow* window,
        const GameplayGamepadInput& gamepad,
        const glm::vec3& cameraFront,
        const glm::vec3& cameraUp);
    void ApplyGamepadCamera(
        const GameplayGamepadInput& gamepad,
        float deltaTime,
        float& yaw,
        float& pitch,
        glm::vec3& cameraFront);
}
