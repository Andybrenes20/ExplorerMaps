#pragma once

#include <string>

#include <glm/glm.hpp>

#include "miniaudio.h"
#include "PhysicsWorld.h"

struct GLFWwindow;
struct GameplayGamepadInput;
class Shader;

class VehicleController {
public:
    bool Load(const std::string& path);
    void UploadToGPU();
    bool InitAudio(ma_engine* engine);
    void Update(
        GLFWwindow* window,
        const GameplayGamepadInput& gamepad,
        float deltaTime,
        PhysicsWorld& city,
        glm::vec3& playerPosition,
        glm::vec3& cameraPosition,
        glm::vec3& cameraFront,
        float& cameraYaw,
        float& cameraPitch);
    void UploadHeadlights(Shader& shader, float nightFactor, float rainIntensity) const;
    void Draw(Shader& shader) const;
    void Shutdown();

    bool IsDriving() const;
    const glm::vec3& GetPosition() const;

private:
    PhysicsWorld model;
    glm::vec3 position = glm::vec3(309.0f, 38.0f, 143.5f);
    float yaw = 0.0f;
    float speed = 0.0f;
    float steering = 0.0f;
    float throttleSmoothed = 0.0f;
    float yawVelocity = 0.0f;
    float bodyRoll = 0.0f;
    float groundPitch = 0.0f;
    float groundRoll = 0.0f;
    float wheelSpin = 0.0f;
    float engineRpm = 0.0f;
    float skidAmount = 0.0f;
    float shiftCooldown = 0.0f;
    int currentGear = 0;
    bool driving = false;
    bool interactWasDown = false;
    bool audioReady[4] = {};
    ma_sound engineIdle = {};
    ma_sound engineMid = {};
    ma_sound engineHigh = {};
    ma_sound skidSound = {};

    glm::mat4 BuildBodyTransform() const;
    glm::mat4 BuildMeshTransform(const RenderMesh& mesh) const;
    void UpdateGroundPose(PhysicsWorld& city, float deltaTime);
    void UpdateAudio(float deltaTime, float steerInput, bool handbrake);
};
