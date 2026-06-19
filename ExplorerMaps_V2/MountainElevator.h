#pragma once

#include "GameplayInput.h"
#include "SkateRampModel.h"

#include <glm/glm.hpp>

class PhysicsWorld;
class Shader;
struct GLFWwindow;

class MountainElevator {
public:
    bool Initialize();
    void Shutdown();
    void Reset();

    void Update(
        GLFWwindow* window,
        const GameplayGamepadInput& gamepad,
        float deltaTime,
        glm::vec3& cameraPosition,
        bool controlsEnabled);

    void Draw(Shader& shader, float currentFrame);
    void DrawHud(float screenWidth, float screenHeight) const;
    void AddCollisionTo(PhysicsWorld& physics) const;

    bool IsRiding() const { return state == State::Moving; }
    bool CanInteract() const { return canInteract; }

private:
    enum class State {
        Idle,
        Moving
    };

    enum class Station {
        Bottom,
        Top
    };

    struct Vertex {
        glm::vec3 position;
        glm::vec2 uv;
        glm::vec3 normal;
    };

    void DrawBox(Shader& shader, const glm::vec3& center, const glm::vec3& size, const glm::vec3& tint);
    void DrawOrientedBox(Shader& shader, const glm::vec3& center, const glm::vec3& size, float yawDegrees, const glm::vec3& tint);
    void DrawSlopedBox(Shader& shader, const glm::vec3& center, const glm::vec3& size, float yawDegrees, float pitchDegrees, const glm::vec3& tint);
    void DrawStation(Shader& shader, const glm::vec3& platform, const glm::vec3& tint, float yawDegrees);
    void DrawCristoEntranceRamps(Shader& shader);
    void DrawMountainBridge(Shader& shader);
    void DrawCabin(Shader& shader, const glm::vec3& platform, float currentFrame, float yawDegrees);
    void AddRampCollision(PhysicsWorld& physics, const glm::vec3& entrance, float yawDegrees) const;
    void AddMountainBridgeCollision(PhysicsWorld& physics) const;
    float DistanceToStation(const glm::vec3& cameraPosition, const glm::vec3& platform) const;
    glm::vec3 CabinPositionForStation(Station station) const;
    glm::vec3 CurrentPlatformPosition() const;
    static float SmoothStep(float value);

    unsigned int cubeVAO = 0;
    unsigned int cubeVBO = 0;
    unsigned int cubeEBO = 0;
    unsigned int whiteTexture = 0;
    SkateRampModel cristoRampModel;

    State state = State::Idle;
    Station nearestStation = Station::Bottom;
    Station destinationStation = Station::Top;
    float rideProgress = 0.0f;
    bool interactWasDown = false;
    bool canInteract = false;

    const glm::vec3 bottomPlatform = glm::vec3(-650.0f, 48.4f, 880.0f);
    const glm::vec3 topPlatform = glm::vec3(-650.0f, 390.4f, 880.0f);
    const glm::vec3 mountainBridgeLanding = glm::vec3(-500.0f, 382.8f, 920.0f);
    const glm::vec3 cristoMainEntrance = glm::vec3(-404.5f, 382.8f, 950.0f);
    const glm::vec3 cristoSideEntrance = glm::vec3(-469.5f, 382.8f, 942.0f);
    const float cristoMainRampYaw = -68.0f;
    const float cristoSideRampYaw = 113.0f;
};
