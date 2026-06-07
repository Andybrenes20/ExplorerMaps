#include "GameplayInput.h"

#include <algorithm>
#include <cmath>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

namespace {
    float ApplyStickDeadzone(float value, float deadzone = 0.18f) {
        if (std::abs(value) <= deadzone) {
            return 0.0f;
        }

        const float sign = value < 0.0f ? -1.0f : 1.0f;
        return sign * std::clamp((std::abs(value) - deadzone) / (1.0f - deadzone), 0.0f, 1.0f);
    }

    float NormalizeTriggerAxis(float value) {
        return std::clamp((value + 1.0f) * 0.5f, 0.0f, 1.0f);
    }
}

namespace GameplayInput {
    GameplayGamepadInput ReadGamepad() {
        // Aqui se leen sticks, gatillos y cuadrado/X del mando.
        GameplayGamepadInput input;
        if (!glfwJoystickIsGamepad(GLFW_JOYSTICK_1)) {
            return input;
        }

        GLFWgamepadstate state;
        if (!glfwGetGamepadState(GLFW_JOYSTICK_1, &state)) {
            return input;
        }

        input.connected = true;
        input.leftX = ApplyStickDeadzone(state.axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
        input.leftY = ApplyStickDeadzone(state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
        input.rightX = ApplyStickDeadzone(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]);
        input.rightY = ApplyStickDeadzone(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
        input.leftTrigger = NormalizeTriggerAxis(state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER]);
        input.rightTrigger = NormalizeTriggerAxis(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER]);
        input.interactDown = state.buttons[GLFW_GAMEPAD_BUTTON_X] == GLFW_PRESS;
        input.pauseDown = state.buttons[GLFW_GAMEPAD_BUTTON_START] == GLFW_PRESS;
        return input;
    }

    bool IsRunning(GLFWwindow* window, const GameplayGamepadInput& gamepad) {
        return glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || gamepad.leftTrigger > 0.55f;
    }

    bool IsMovementPressed(GLFWwindow* window, const GameplayGamepadInput& gamepad) {
        return glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS ||
            std::abs(gamepad.leftX) > 0.01f ||
            std::abs(gamepad.leftY) > 0.01f ||
            gamepad.rightTrigger > 0.05f;
    }

    glm::vec3 BuildMoveDirection(
        GLFWwindow* window,
        const GameplayGamepadInput& gamepad,
        const glm::vec3& cameraFront,
        const glm::vec3& cameraUp) {
        glm::vec3 frontFlat(cameraFront.x, 0.0f, cameraFront.z);
        if (glm::length(frontFlat) <= 0.0001f) {
            frontFlat = glm::vec3(0.0f, 0.0f, -1.0f);
        }
        frontFlat = glm::normalize(frontFlat);

        glm::vec3 rightFlat = glm::cross(frontFlat, cameraUp);
        if (glm::length(rightFlat) <= 0.0001f) {
            rightFlat = glm::vec3(1.0f, 0.0f, 0.0f);
        }
        else {
            rightFlat = glm::normalize(rightFlat);
        }

        glm::vec3 moveDir(0.0f);
        if ((glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)) moveDir += frontFlat;
        if ((glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)) moveDir -= frontFlat;
        if ((glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)) moveDir -= rightFlat;
        if ((glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)) moveDir += rightFlat;

        moveDir += rightFlat * gamepad.leftX;
        moveDir += frontFlat * -gamepad.leftY;
        moveDir += frontFlat * gamepad.rightTrigger;

        if (glm::length(moveDir) > 0.0001f) {
            return glm::normalize(moveDir);
        }
        return glm::vec3(0.0f);
    }

    void ApplyGamepadCamera(
        const GameplayGamepadInput& gamepad,
        float deltaTime,
        float& yaw,
        float& pitch,
        glm::vec3& cameraFront) {
        if (!gamepad.connected) {
            return;
        }

        // Aqui el stick derecho controla la camara como en el proyecto original.
        constexpr float yawSpeed = 145.0f;
        constexpr float pitchSpeed = 95.0f;
        yaw += gamepad.rightX * yawSpeed * deltaTime;
        pitch -= gamepad.rightY * pitchSpeed * deltaTime;
        pitch = std::clamp(pitch, -89.0f, 89.0f);

        glm::vec3 front;
        front.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        front.y = std::sin(glm::radians(pitch));
        front.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        cameraFront = glm::normalize(front);
    }
}
