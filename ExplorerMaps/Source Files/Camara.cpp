#include <cmath>
#include <iostream>
#include "Camara.h"

Camera::Camera(int width, int height, glm::vec3 position)
{
    Camera::width = width;
    Camera::height = height;
    Position = position;
}

void Camera::updateMatrix(float FOVdeg, float nearPlane, float farPlane)
{
    // Initializes matrices since otherwise they will be the null matrix
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);

    // Makes camera look in the right direction from the right position
    view = glm::lookAt(Position, Position + Orientation, Up);
    // Adds perspective to the scene
    projection = glm::perspective(glm::radians(FOVdeg), (float)width / height, nearPlane, farPlane);

    // Sets new camera matrix
    cameraMatrix = projection * view;
}

void Camera::Matrix(Shader& shader, const char* uniform)
{
    // Exports camera matrix
    glUniformMatrix4fv(shader.GetUniformLocation(uniform), 1, GL_FALSE, glm::value_ptr(cameraMatrix));
}

glm::mat4 Camera::GetViewMatrix() const
{
    return glm::lookAt(Position, Position + Orientation, Up);
}

glm::mat4 Camera::GetProjectionMatrix(float FOVdeg, float nearPlane, float farPlane) const
{
    return glm::perspective(glm::radians(FOVdeg), static_cast<float>(width) / static_cast<float>(height), nearPlane, farPlane);
}

void Camera::Inputs(GLFWwindow* window, float deltaTime)
{
    // Toggle fly mode with F key
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
    {
        if (!toggleLatch)
        {
            flyMode = !flyMode;
            toggleLatch = true;
        }
    }
    else
    {
        toggleLatch = false;
    }

    float currentSpeed = speed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
    {
        currentSpeed *= 1.45f;
    }

    // Calculate movement directions based on Orientation
    glm::vec3 forward = Orientation;
    if (glm::length(forward) > 0.0001f)
    {
        forward = glm::normalize(forward);
    }
    else
    {
        forward = glm::vec3(0.0f, 0.0f, -1.0f);
    }

    glm::vec3 moveForward = forward;
    if (!flyMode)
    {
        moveForward = glm::vec3(forward.x, 0.0f, forward.z);
        if (glm::length(moveForward) > 0.0001f)
        {
            moveForward = glm::normalize(moveForward);
        }
        else
        {
            moveForward = glm::vec3(0.0f, 0.0f, -1.0f);
        }
    }

    glm::vec3 right = glm::cross(moveForward, Up);
    if (glm::length(right) > 0.0001f)
    {
        right = glm::normalize(right);
    }
    else
    {
        right = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    // WASD movement
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        Position += currentSpeed * moveForward;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        Position += currentSpeed * -right;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        Position += currentSpeed * -moveForward;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        Position += currentSpeed * right;
    }
    if (flyMode && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
    {
        Position += currentSpeed * Up;
    }
    if (flyMode && glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
    {
        Position += currentSpeed * -Up;
    }

    // Mouse look
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (firstClick)
    {
        glfwSetCursorPos(window, (width / 2), (height / 2));
        firstClick = false;
    }

    double mouseX;
    double mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    float rotX = sensitivity * (float)(mouseY - (height / 2)) / height;
    float rotY = sensitivity * (float)(mouseX - (width / 2)) / width;

    // Pitch rotation (up/down)
    glm::vec3 pitchAxis = glm::cross(forward, Up);
    if (glm::length(pitchAxis) > 0.0001f)
    {
        pitchAxis = glm::normalize(pitchAxis);
    }
    else
    {
        pitchAxis = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    glm::mat4 rotationMatrixX = glm::rotate(glm::mat4(1.0f), glm::radians(-rotX), pitchAxis);
    glm::vec3 newOrientation = glm::vec3(rotationMatrixX * glm::vec4(forward, 0.0f));

    float orientationDot = glm::clamp(glm::dot(glm::normalize(newOrientation), glm::normalize(Up)), -1.0f, 1.0f);
    float orientationAngle = std::acos(orientationDot);
    if (abs(orientationAngle - glm::radians(90.0f)) <= glm::radians(85.0f))
    {
        forward = glm::normalize(newOrientation);
    }

    // Yaw rotation (left/right)
    glm::mat4 rotationMatrixY = glm::rotate(glm::mat4(1.0f), glm::radians(-rotY), Up);
    Orientation = glm::normalize(glm::vec3(rotationMatrixY * glm::vec4(forward, 0.0f)));

    // Also update forward for the next frame
    forward = Orientation;

    glfwSetCursorPos(window, (width / 2), (height / 2));

    // ==========================================
    // GAMEPAD HANDLING - CON CONDICIÓN MEJORADA
    // ==========================================

    // Variable estática para recordar el estado anterior del mando
    static bool gamepadConnected = false;
    static float gamepadCheckTimer = 0.0f;
    static bool firstConnectionMessage = true;

    // Verificar si hay un mando conectado (cada 0.5 segundos para optimizar)
    gamepadCheckTimer += deltaTime;

    if (gamepadCheckTimer >= 0.5f)
    {
        gamepadCheckTimer = 0.0f;
        bool wasConnected = gamepadConnected;
        gamepadConnected = glfwJoystickIsGamepad(GLFW_JOYSTICK_1);

        // Mostrar mensaje cuando se conecta/desconecta el mando
        if (wasConnected != gamepadConnected)
        {
            if (gamepadConnected)
            {
                std::cout << "🎮 Gamepad connected! Player can use controller." << std::endl;
                std::cout << "   - Left stick: Move" << std::endl;
                std::cout << "   - Right stick: Look around" << std::endl;
                std::cout << "   - Button A: Move up" << std::endl;
                std::cout << "   - Button B: Move down" << std::endl;
                std::cout << "   - L1 / Left bumper: Sprint" << std::endl;
                std::cout << "   - R1 / Right bumper: Toggle fly mode" << std::endl;
            }
            else
            {
                std::cout << "🎮 Gamepad disconnected! Using keyboard/mouse only." << std::endl;
            }
        }
        else if (gamepadConnected && firstConnectionMessage)
        {
            // Mensaje la primera vez que se detecta el mando
            std::cout << "🎮 Gamepad detected! Use controller for camera movement." << std::endl;
            std::cout << "   - Left stick: Move" << std::endl;
            std::cout << "   - Right stick: Look around" << std::endl;
            std::cout << "   - Button A: Move up" << std::endl;
            std::cout << "   - Button B: Move down" << std::endl;
            std::cout << "   - L1 / Left bumper: Sprint" << std::endl;
            std::cout << "   - R1 / Right bumper: Toggle fly mode" << std::endl;
            firstConnectionMessage = false;
        }
    }

    // SOLO PROCESAR EL MANDO SI ESTÁ CONECTADO
    if (gamepadConnected)
    {
        GLFWgamepadstate state;

        if (glfwGetGamepadState(GLFW_JOYSTICK_1, &state))
        {
            const float deadzone = 0.20f;
            static bool rightBumperLatch = false;

            auto dz = [deadzone](float value)
                {
                    return std::abs(value) < deadzone ? 0.0f : value;
                };

            float leftX = dz(state.axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
            float leftY = dz(state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
            float rightX = dz(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]);
            float rightY = dz(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
            float gamepadSpeed = currentSpeed;

            if (state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] == GLFW_PRESS)
            {
                gamepadSpeed *= 1.30f;
            }

            if (state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] == GLFW_PRESS)
            {
                if (!rightBumperLatch)
                {
                    flyMode = !flyMode;
                    rightBumperLatch = true;
                }
            }
            else
            {
                rightBumperLatch = false;
            }

            // MOVIMIENTO CON EL STICK IZQUIERDO
            if (leftX != 0.0f || leftY != 0.0f)
            {
                Position += gamepadSpeed * leftX * right;
                Position += gamepadSpeed * -leftY * moveForward;
            }

            // MIRAR CON EL STICK DERECHO
            if (rightX != 0.0f || rightY != 0.0f)
            {
                float lookSpeed = sensitivity * deltaTime * 1.7f;

                // Recalcular forward actual basado en Orientation
                glm::vec3 currentForward = Orientation;
                if (glm::length(currentForward) > 0.0001f)
                    currentForward = glm::normalize(currentForward);
                else
                    currentForward = glm::vec3(0.0f, 0.0f, -1.0f);

                // Pitch rotation (arriba/abajo)
                glm::vec3 pitchAxis = glm::cross(currentForward, Up);
                if (glm::length(pitchAxis) > 0.0001f)
                    pitchAxis = glm::normalize(pitchAxis);
                else
                    pitchAxis = glm::vec3(1.0f, 0.0f, 0.0f);

                glm::mat4 rotationX = glm::rotate(
                    glm::mat4(1.0f),
                    glm::radians(-rightY * lookSpeed),
                    pitchAxis
                );

                glm::vec3 newOrientation = glm::vec3(rotationX * glm::vec4(currentForward, 0.0f));

                float orientationDot = glm::clamp(
                    glm::dot(glm::normalize(newOrientation), glm::normalize(Up)),
                    -1.0f,
                    1.0f
                );

                float orientationAngle = std::acos(orientationDot);

                if (abs(orientationAngle - glm::radians(90.0f)) <= glm::radians(85.0f))
                {
                    currentForward = glm::normalize(newOrientation);
                }

                // Yaw rotation (izquierda/derecha)
                glm::mat4 rotationY = glm::rotate(
                    glm::mat4(1.0f),
                    glm::radians(-rightX * lookSpeed),
                    Up
                );

                Orientation = glm::normalize(glm::vec3(rotationY * glm::vec4(currentForward, 0.0f)));
            }

            // BOTONES DE MOVIMIENTO VERTICAL
            // Botón A / Cross: subir
            if (state.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS)
            {
                Position += gamepadSpeed * Up;
            }

            // Botón B / Circle: bajar
            if (state.buttons[GLFW_GAMEPAD_BUTTON_B] == GLFW_PRESS)
            {
                Position += gamepadSpeed * -Up;
            }
        }
    }
}
