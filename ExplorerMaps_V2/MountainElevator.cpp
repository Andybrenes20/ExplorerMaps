#include "MountainElevator.h"

#include "PhysicsWorld.h"
#include "Shader.h"
#include "imgui/imgui.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <vector>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

namespace {
    constexpr float PLAYER_HEIGHT = 1.75f;
    constexpr float RIDE_DURATION = 9.0f;
    constexpr float INTERACT_RADIUS = 16.0f;
    constexpr float INTERACT_HEIGHT = 24.0f;

    const glm::vec3 kConcrete = glm::vec3(0.54f, 0.56f, 0.58f);
    const glm::vec3 kMetal = glm::vec3(0.36f, 0.40f, 0.43f);
    const glm::vec3 kDarkMetal = glm::vec3(0.15f, 0.17f, 0.18f);
    const glm::vec3 kWarningYellow = glm::vec3(1.0f, 0.74f, 0.20f);
    const glm::vec3 kCabinBlue = glm::vec3(0.18f, 0.48f, 0.78f);
    const glm::vec3 kGlassBlue = glm::vec3(0.34f, 0.62f, 0.78f);
    const glm::vec3 kSoftLight = glm::vec3(0.86f, 0.94f, 1.0f);

    glm::vec3 RotateY(const glm::vec3& offset, float yawDegrees) {
        const float yaw = glm::radians(yawDegrees);
        const float c = std::cos(yaw);
        const float s = std::sin(yaw);
        return glm::vec3(
            offset.x * c + offset.z * s,
            offset.y,
            -offset.x * s + offset.z * c);
    }

    float YawForHorizontalDelta(const glm::vec3& delta) {
        return glm::degrees(std::atan2(-delta.z, delta.x));
    }

    glm::vec3 RotateYawPitch(const glm::vec3& offset, float yawDegrees, float pitchDegrees) {
        const float pitch = glm::radians(pitchDegrees);
        const float c = std::cos(pitch);
        const float s = std::sin(pitch);
        const glm::vec3 pitched(
            offset.x * c - offset.y * s,
            offset.x * s + offset.y * c,
            offset.z);
        return RotateY(pitched, yawDegrees);
    }
}

bool MountainElevator::Initialize() {
    const std::array<Vertex, 24> vertices = {
        Vertex{{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        Vertex{{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        Vertex{{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        Vertex{{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},

        Vertex{{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        Vertex{{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        Vertex{{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        Vertex{{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},

        Vertex{{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
        Vertex{{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
        Vertex{{-0.5f,  0.5f,  0.5f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
        Vertex{{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},

        Vertex{{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        Vertex{{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        Vertex{{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        Vertex{{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},

        Vertex{{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        Vertex{{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        Vertex{{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        Vertex{{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},

        Vertex{{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
        Vertex{{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
        Vertex{{ 0.5f, -0.5f,  0.5f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
        Vertex{{-0.5f, -0.5f,  0.5f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
    };

    const std::array<unsigned int, 36> indices = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        8, 9, 10, 10, 11, 8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20
    };

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);
    glBindVertexArray(cubeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);

    const unsigned char whitePixel[] = { 255, 255, 255, 255 };
    glGenTextures(1, &whiteTexture);
    glBindTexture(GL_TEXTURE_2D, whiteTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    cristoRampModel.Load("modelos/Skate_Ramp_Fun_Box_07_OBJ/Skate_Ramp_Fun_Box_07_.obj");
    Reset();
    return cubeVAO != 0 && whiteTexture != 0;
}

void MountainElevator::Shutdown() {
    cristoRampModel.Shutdown();
    if (cubeVAO != 0) glDeleteVertexArrays(1, &cubeVAO);
    if (cubeVBO != 0) glDeleteBuffers(1, &cubeVBO);
    if (cubeEBO != 0) glDeleteBuffers(1, &cubeEBO);
    if (whiteTexture != 0) glDeleteTextures(1, &whiteTexture);
    cubeVAO = cubeVBO = cubeEBO = whiteTexture = 0;
}

void MountainElevator::Reset() {
    state = State::Idle;
    nearestStation = Station::Bottom;
    destinationStation = Station::Top;
    rideProgress = 0.0f;
    interactWasDown = false;
    canInteract = false;
}

void MountainElevator::Update(
    GLFWwindow* window,
    const GameplayGamepadInput& gamepad,
    float deltaTime,
    glm::vec3& cameraPosition,
    bool controlsEnabled) {
    const bool interactDown = controlsEnabled &&
        (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS || gamepad.interactDown);

    if (state == State::Moving) {
        rideProgress = std::min(rideProgress + deltaTime / RIDE_DURATION, 1.0f);
        const glm::vec3 platform = CurrentPlatformPosition();
        cameraPosition = platform + glm::vec3(0.0f, PLAYER_HEIGHT + 0.12f, 0.0f);
        canInteract = false;

        if (rideProgress >= 1.0f) {
            state = State::Idle;
            nearestStation = destinationStation;
            cameraPosition = (nearestStation == Station::Bottom ? bottomPlatform : topPlatform) +
                glm::vec3(0.0f, PLAYER_HEIGHT + 0.12f, 0.0f);
        }

        interactWasDown = interactDown;
        return;
    }

    const float bottomDistance = DistanceToStation(cameraPosition, bottomPlatform);
    const float topDistance = DistanceToStation(cameraPosition, topPlatform);
    nearestStation = bottomDistance <= topDistance ? Station::Bottom : Station::Top;
    const float nearestDistance = std::min(bottomDistance, topDistance);
    const glm::vec3 station = nearestStation == Station::Bottom ? bottomPlatform : topPlatform;
    const float verticalDistance = std::abs(cameraPosition.y - (station.y + PLAYER_HEIGHT));
    canInteract = controlsEnabled && nearestDistance <= INTERACT_RADIUS && verticalDistance <= INTERACT_HEIGHT;

    if (canInteract && interactDown && !interactWasDown) {
        state = State::Moving;
        destinationStation = nearestStation == Station::Bottom ? Station::Top : Station::Bottom;
        rideProgress = 0.0f;
        cameraPosition = CabinPositionForStation(nearestStation) + glm::vec3(0.0f, PLAYER_HEIGHT + 0.12f, 0.0f);
    }

    interactWasDown = interactDown;
}

void MountainElevator::Draw(Shader& shader, float currentFrame) {
    if (cubeVAO == 0) {
        return;
    }

    shader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, whiteTexture);

    const float elevatorYaw = YawForHorizontalDelta(mountainBridgeLanding - topPlatform);
    DrawStation(shader, bottomPlatform, kWarningYellow, elevatorYaw);
    DrawStation(shader, topPlatform, kCabinBlue, elevatorYaw);
    DrawMountainBridge(shader);
    DrawCristoEntranceRamps(shader);

    for (int i = 0; i <= 24; ++i) {
        const float t = static_cast<float>(i) / 24.0f;
        const glm::vec3 point = glm::mix(bottomPlatform, topPlatform, t);
        DrawOrientedBox(shader, point + RotateY(glm::vec3(-4.4f, 3.2f, -4.4f), elevatorYaw), glm::vec3(0.72f, 0.72f, 0.72f), elevatorYaw, kDarkMetal);
        DrawOrientedBox(shader, point + RotateY(glm::vec3(4.4f, 3.2f, -4.4f), elevatorYaw), glm::vec3(0.72f, 0.72f, 0.72f), elevatorYaw, kDarkMetal);
        DrawOrientedBox(shader, point + RotateY(glm::vec3(-4.4f, 3.2f, 4.4f), elevatorYaw), glm::vec3(0.72f, 0.72f, 0.72f), elevatorYaw, kDarkMetal);
        DrawOrientedBox(shader, point + RotateY(glm::vec3(4.4f, 3.2f, 4.4f), elevatorYaw), glm::vec3(0.72f, 0.72f, 0.72f), elevatorYaw, kDarkMetal);
        if (i % 4 == 0) {
            DrawOrientedBox(shader, point + RotateY(glm::vec3(0.0f, 3.2f, -4.4f), elevatorYaw), glm::vec3(9.6f, 0.28f, 0.30f), elevatorYaw, kMetal);
            DrawOrientedBox(shader, point + RotateY(glm::vec3(0.0f, 3.2f, 4.4f), elevatorYaw), glm::vec3(9.6f, 0.28f, 0.30f), elevatorYaw, kMetal);
            DrawOrientedBox(shader, point + RotateY(glm::vec3(-4.4f, 3.2f, 0.0f), elevatorYaw), glm::vec3(0.30f, 0.28f, 9.6f), elevatorYaw, kMetal);
            DrawOrientedBox(shader, point + RotateY(glm::vec3(4.4f, 3.2f, 0.0f), elevatorYaw), glm::vec3(0.30f, 0.28f, 9.6f), elevatorYaw, kMetal);
        }
    }

    const float shaftHeight = topPlatform.y - bottomPlatform.y + 18.0f;
    const glm::vec3 shaftCenter = (bottomPlatform + topPlatform) * 0.5f + glm::vec3(0.0f, 8.5f, 0.0f);
    DrawOrientedBox(shader, shaftCenter + RotateY(glm::vec3(-2.1f, 0.0f, 0.0f), elevatorYaw), glm::vec3(0.18f, shaftHeight, 0.18f), elevatorYaw, kSoftLight);
    DrawOrientedBox(shader, shaftCenter + RotateY(glm::vec3(2.1f, 0.0f, 0.0f), elevatorYaw), glm::vec3(0.18f, shaftHeight, 0.18f), elevatorYaw, kSoftLight);
    DrawOrientedBox(shader, topPlatform + glm::vec3(0.0f, 18.0f, 0.0f), glm::vec3(12.4f, 1.4f, 12.4f), elevatorYaw, kDarkMetal);
    DrawOrientedBox(shader, topPlatform + glm::vec3(0.0f, 19.0f, 0.0f), glm::vec3(8.0f, 1.0f, 6.6f), elevatorYaw, kMetal);

    const glm::vec3 cabinPlatform = state == State::Moving ? CurrentPlatformPosition() :
        CabinPositionForStation(nearestStation);
    DrawCabin(shader, cabinPlatform, currentFrame, elevatorYaw);

    shader.setVec3("objectTint", glm::vec3(1.0f));
    shader.setMat4("model", glm::mat4(1.0f));
    glBindVertexArray(0);
}

void MountainElevator::DrawHud(float screenWidth, float screenHeight) const {
    if (!canInteract && state != State::Moving) {
        return;
    }

    ImDrawList* drawList = ImGui::GetOverlayDrawList();
    const ImGuiIO& io = ImGui::GetIO();
    const float width = io.DisplaySize.x > 0.0f ? io.DisplaySize.x : screenWidth;
    const float height = io.DisplaySize.y > 0.0f ? io.DisplaySize.y : screenHeight;
    const char* text = state == State::Moving
        ? "Ascensor al Cristo"
        : (nearestStation == Station::Bottom ? "E / X  Subir al Cristo" : "E / X  Bajar a la base");

    const ImVec2 textSize = ImGui::CalcTextSize(text);
    const ImVec2 pos(width * 0.5f - textSize.x * 0.5f, height * 0.72f);
    drawList->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 190), text);
    drawList->AddText(pos, state == State::Moving ? IM_COL32(170, 220, 255, 245) : IM_COL32(255, 220, 115, 245), text);

    if (state == State::Moving) {
        const float barWidth = 220.0f;
        const float barHeight = 5.0f;
        const ImVec2 barMin(width * 0.5f - barWidth * 0.5f, pos.y + textSize.y + 9.0f);
        const ImVec2 barMax(barMin.x + barWidth, barMin.y + barHeight);
        drawList->AddRectFilled(barMin, barMax, IM_COL32(0, 0, 0, 150), 2.0f);
        drawList->AddRectFilled(barMin, ImVec2(barMin.x + barWidth * SmoothStep(rideProgress), barMax.y), IM_COL32(92, 176, 255, 230), 2.0f);
    }
}

void MountainElevator::DrawBox(Shader& shader, const glm::vec3& center, const glm::vec3& size, const glm::vec3& tint) {
    const glm::mat4 model = glm::translate(glm::mat4(1.0f), center) * glm::scale(glm::mat4(1.0f), size);
    shader.setMat4("model", model);
    shader.setVec3("objectTint", tint);
    glBindVertexArray(cubeVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
}

void MountainElevator::DrawOrientedBox(Shader& shader, const glm::vec3& center, const glm::vec3& size, float yawDegrees, const glm::vec3& tint) {
    const glm::mat4 model =
        glm::translate(glm::mat4(1.0f), center) *
        glm::rotate(glm::mat4(1.0f), glm::radians(yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f)) *
        glm::scale(glm::mat4(1.0f), size);
    shader.setMat4("model", model);
    shader.setVec3("objectTint", tint);
    glBindVertexArray(cubeVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
}

void MountainElevator::DrawSlopedBox(Shader& shader, const glm::vec3& center, const glm::vec3& size, float yawDegrees, float pitchDegrees, const glm::vec3& tint) {
    const glm::mat4 model =
        glm::translate(glm::mat4(1.0f), center) *
        glm::rotate(glm::mat4(1.0f), glm::radians(yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f)) *
        glm::rotate(glm::mat4(1.0f), glm::radians(pitchDegrees), glm::vec3(0.0f, 0.0f, 1.0f)) *
        glm::scale(glm::mat4(1.0f), size);
    shader.setMat4("model", model);
    shader.setVec3("objectTint", tint);
    glBindVertexArray(cubeVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
}

void MountainElevator::DrawStation(Shader& shader, const glm::vec3& platform, const glm::vec3& tint, float yawDegrees) {
    const auto drawPart = [&](const glm::vec3& offset, const glm::vec3& size, const glm::vec3& color) {
        DrawOrientedBox(shader, platform + RotateY(offset, yawDegrees), size, yawDegrees, color);
    };

    drawPart(glm::vec3(0.0f, -0.30f, 0.0f), glm::vec3(13.2f, 0.50f, 13.2f), kConcrete);
    drawPart(glm::vec3(0.0f, -0.02f, 0.0f), glm::vec3(9.6f, 0.16f, 9.6f), glm::vec3(0.43f, 0.45f, 0.46f));
    drawPart(glm::vec3(0.0f, 0.12f, -4.9f), glm::vec3(8.4f, 0.30f, 0.42f), tint);
    drawPart(glm::vec3(0.0f, 3.5f, -5.2f), glm::vec3(9.0f, 0.38f, 0.38f), kMetal);
    drawPart(glm::vec3(-4.5f, 1.8f, -5.2f), glm::vec3(0.36f, 3.6f, 0.36f), kMetal);
    drawPart(glm::vec3(4.5f, 1.8f, -5.2f), glm::vec3(0.36f, 3.6f, 0.36f), kMetal);
    drawPart(glm::vec3(0.0f, 5.6f, -1.8f), glm::vec3(10.6f, 0.45f, 7.2f), kDarkMetal);
    drawPart(glm::vec3(0.0f, 5.95f, -1.8f), glm::vec3(9.2f, 0.24f, 5.8f), tint);
    drawPart(glm::vec3(-5.0f, 2.8f, 2.8f), glm::vec3(0.28f, 5.2f, 0.28f), kMetal);
    drawPart(glm::vec3(5.0f, 2.8f, 2.8f), glm::vec3(0.28f, 5.2f, 0.28f), kMetal);
    drawPart(glm::vec3(0.0f, 2.3f, 5.0f), glm::vec3(9.2f, 0.24f, 0.24f), kMetal);
    drawPart(glm::vec3(-5.1f, 7.0f, 0.0f), glm::vec3(0.55f, 14.0f, 0.55f), tint);
    drawPart(glm::vec3(-5.1f, 14.4f, 0.0f), glm::vec3(3.2f, 0.85f, 3.2f), tint);
}

void MountainElevator::DrawCristoEntranceRamps(Shader& shader) {
    const auto drawEntranceRamp = [&](const glm::vec3& entrance, float yawDegrees) {
        cristoRampModel.Draw(shader, entrance + glm::vec3(-6.4f, -0.18f, 0.0f), yawDegrees, 0.078f, glm::vec3(0.78f, 0.80f, 0.78f));
    };

    drawEntranceRamp(cristoMainEntrance, cristoMainRampYaw);
    drawEntranceRamp(cristoSideEntrance, cristoSideRampYaw);
}

void MountainElevator::DrawMountainBridge(Shader& shader) {
    const glm::vec3 delta = mountainBridgeLanding - topPlatform;
    const float horizontalLength = glm::length(glm::vec2(delta.x, delta.z));
    const float yaw = YawForHorizontalDelta(delta);
    const float pitch = glm::degrees(std::atan2(delta.y, horizontalLength));
    const glm::vec3 center = (topPlatform + mountainBridgeLanding) * 0.5f + glm::vec3(0.0f, 0.08f, 0.0f);
    const float width = 10.5f;

    DrawSlopedBox(shader, center, glm::vec3(horizontalLength + 8.0f, 0.36f, width), yaw, pitch, kConcrete);
    DrawSlopedBox(shader, center + RotateYawPitch(glm::vec3(0.0f, 2.0f, -width * 0.5f), yaw, pitch),
        glm::vec3(horizontalLength + 8.0f, 0.28f, 0.26f), yaw, pitch, kMetal);
    DrawSlopedBox(shader, center + RotateYawPitch(glm::vec3(0.0f, 2.0f, width * 0.5f), yaw, pitch),
        glm::vec3(horizontalLength + 8.0f, 0.28f, 0.26f), yaw, pitch, kMetal);

    for (int i = 0; i <= 8; ++i) {
        const float t = static_cast<float>(i) / 8.0f;
        const glm::vec3 along = glm::mix(topPlatform, mountainBridgeLanding, t);
        DrawOrientedBox(shader, along + RotateY(glm::vec3(0.0f, 1.0f, -width * 0.5f), yaw),
            glm::vec3(0.24f, 2.0f, 0.24f), yaw, kMetal);
        DrawOrientedBox(shader, along + RotateY(glm::vec3(0.0f, 1.0f, width * 0.5f), yaw),
            glm::vec3(0.24f, 2.0f, 0.24f), yaw, kMetal);
    }
}

void MountainElevator::AddCollisionTo(PhysicsWorld& physics) const {
    const auto addStationCollision = [&](const glm::vec3& platform) {
        physics.AddCollisionBox(platform + glm::vec3(0.0f, -0.18f, 0.0f), glm::vec3(10.5f, 0.35f, 10.5f));
    };

    addStationCollision(bottomPlatform);
    addStationCollision(topPlatform);
    AddMountainBridgeCollision(physics);
    AddRampCollision(physics, cristoMainEntrance, cristoMainRampYaw);
    AddRampCollision(physics, cristoSideEntrance, cristoSideRampYaw);
}

void MountainElevator::AddRampCollision(PhysicsWorld& physics, const glm::vec3& entrance, float yawDegrees) const {
    cristoRampModel.AddCollisionTo(
        physics,
        entrance + glm::vec3(-6.4f, -0.18f, 0.0f),
        yawDegrees,
        0.078f);
}

void MountainElevator::AddMountainBridgeCollision(PhysicsWorld& physics) const {
    const glm::vec3 delta = mountainBridgeLanding - topPlatform;
    const float yaw = YawForHorizontalDelta(delta);
    const float horizontalLength = glm::length(glm::vec2(delta.x, delta.z));
    const float pitch = glm::degrees(std::atan2(delta.y, horizontalLength));
    const float length = horizontalLength + 8.0f;
    const float halfLength = length * 0.5f;
    const float halfWidth = 5.25f;
    const glm::vec3 center = (topPlatform + mountainBridgeLanding) * 0.5f + glm::vec3(0.0f, 0.08f, 0.0f);

    const float thickness = 0.42f;
    const auto point = [&](float x, float y, float z) {
        return center + RotateYawPitch(glm::vec3(x, y, z), yaw, pitch);
    };

    physics.AddCollisionTriangles({
        point(-halfLength, 0.0f, -halfWidth),
        point(halfLength, 0.0f, -halfWidth),
        point(halfLength, 0.0f, halfWidth),
        point(-halfLength, 0.0f, halfWidth),
        point(-halfLength, -thickness, -halfWidth),
        point(halfLength, -thickness, -halfWidth),
        point(halfLength, -thickness, halfWidth),
        point(-halfLength, -thickness, halfWidth)
    }, {
        0, 1, 2, 2, 3, 0,
        4, 7, 6, 6, 5, 4,
        0, 4, 5, 5, 1, 0,
        3, 2, 6, 6, 7, 3,
        0, 3, 7, 7, 4, 0,
        1, 5, 6, 6, 2, 1
    });

    physics.AddCollisionBox(topPlatform + glm::vec3(0.0f, 0.02f, 0.0f), glm::vec3(12.0f, 0.35f, 12.0f));
    physics.AddCollisionBox(mountainBridgeLanding + glm::vec3(0.0f, 0.02f, 0.0f), glm::vec3(14.0f, 0.35f, 14.0f));
}

void MountainElevator::DrawCabin(Shader& shader, const glm::vec3& platform, float currentFrame, float yawDegrees) {
    const float lightPulse = 0.5f + 0.5f * std::sin(currentFrame * 4.2f);
    const glm::vec3 accent = glm::mix(kCabinBlue, glm::vec3(0.36f, 0.72f, 1.0f), lightPulse * 0.35f);
    const auto drawPart = [&](const glm::vec3& offset, const glm::vec3& size, const glm::vec3& color) {
        DrawOrientedBox(shader, platform + RotateY(offset, yawDegrees), size, yawDegrees, color);
    };

    drawPart(glm::vec3(0.0f, 0.12f, 0.0f), glm::vec3(9.2f, 0.34f, 9.2f), kDarkMetal);
    drawPart(glm::vec3(0.0f, 0.34f, 0.0f), glm::vec3(8.2f, 0.16f, 8.2f), glm::vec3(0.43f, 0.45f, 0.46f));
    drawPart(glm::vec3(0.0f, 3.30f, 0.0f), glm::vec3(9.0f, 0.45f, 9.0f), kDarkMetal);
    drawPart(glm::vec3(0.0f, 3.62f, 0.0f), glm::vec3(7.2f, 0.24f, 7.2f), accent);

    drawPart(glm::vec3(-4.15f, 1.72f, -4.15f), glm::vec3(0.42f, 3.25f, 0.42f), kMetal);
    drawPart(glm::vec3(4.15f, 1.72f, -4.15f), glm::vec3(0.42f, 3.25f, 0.42f), kMetal);
    drawPart(glm::vec3(-4.15f, 1.72f, 4.15f), glm::vec3(0.42f, 3.25f, 0.42f), kMetal);
    drawPart(glm::vec3(4.15f, 1.72f, 4.15f), glm::vec3(0.42f, 3.25f, 0.42f), kMetal);

    drawPart(glm::vec3(-2.15f, 1.85f, -4.22f), glm::vec3(3.2f, 2.35f, 0.16f), kGlassBlue);
    drawPart(glm::vec3(2.15f, 1.85f, -4.22f), glm::vec3(3.2f, 2.35f, 0.16f), kGlassBlue);
    drawPart(glm::vec3(0.0f, 1.85f, 4.22f), glm::vec3(6.7f, 2.35f, 0.16f), kGlassBlue);
    drawPart(glm::vec3(-4.22f, 1.85f, 0.0f), glm::vec3(0.16f, 2.35f, 6.7f), kGlassBlue);
    drawPart(glm::vec3(4.22f, 1.85f, 0.0f), glm::vec3(0.16f, 2.35f, 6.7f), kGlassBlue);

    drawPart(glm::vec3(0.0f, 1.45f, -4.34f), glm::vec3(0.20f, 2.55f, 0.18f), accent);
    drawPart(glm::vec3(0.0f, 3.05f, -4.35f), glm::vec3(7.2f, 0.20f, 0.20f), accent);
    drawPart(glm::vec3(0.0f, 0.78f, -4.35f), glm::vec3(7.2f, 0.20f, 0.20f), accent);
    drawPart(glm::vec3(0.0f, 2.15f, 0.0f), glm::vec3(0.24f, 0.24f, 7.8f), kSoftLight);
    drawPart(glm::vec3(0.0f, 3.95f, 0.0f), glm::vec3(2.2f, 0.28f, 2.2f), kSoftLight);
}

float MountainElevator::DistanceToStation(const glm::vec3& cameraPosition, const glm::vec3& platform) const {
    const glm::vec2 cameraFlat(cameraPosition.x, cameraPosition.z);
    const glm::vec2 platformFlat(platform.x, platform.z);
    return glm::length(cameraFlat - platformFlat);
}

glm::vec3 MountainElevator::CabinPositionForStation(Station station) const {
    return station == Station::Bottom ? bottomPlatform : topPlatform;
}

glm::vec3 MountainElevator::CurrentPlatformPosition() const {
    const float t = SmoothStep(rideProgress);
    if (destinationStation == Station::Top) {
        return glm::mix(bottomPlatform, topPlatform, t);
    }
    return glm::mix(topPlatform, bottomPlatform, t);
}

float MountainElevator::SmoothStep(float value) {
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
