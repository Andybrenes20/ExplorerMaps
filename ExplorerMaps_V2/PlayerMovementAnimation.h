#pragma once

#include <glm/glm.hpp>

struct GLFWwindow;
struct GameplayGamepadInput;

// Funcionalidad: animacion visual de caminar/correr.
// Implementacion completa en PlayerMovementAnimation.cpp.
class PlayerMovementAnimation {
public:
    void Update(GLFWwindow* window, const GameplayGamepadInput& gamepad, float deltaTime, const glm::vec3& cameraFront, const glm::vec3& cameraUp);
    glm::vec3 GetCameraOffset() const;
    bool IsMoving() const;
    bool IsRunning() const;

private:
    float phase = 0.0f;
    float blend = 0.0f;
    glm::vec3 cameraOffset = glm::vec3(0.0f);
    bool moving = false;
    bool running = false;
};
