#include "VehicleController.h"

#include <algorithm>
#include <cmath>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "GameplayInput.h"
#include "Shader.h"

namespace {
    constexpr float VEHICLE_SCALE = 1.55f;
    constexpr float MAX_FORWARD_SPEED = 47.0f;
    constexpr float MAX_REVERSE_SPEED = 9.0f;
    constexpr float IDLE_RPM = 0.12f;
    constexpr float GEAR_MIN_SPEED[] = { 0.0f, 8.5f, 16.0f, 25.0f, 34.0f };
    constexpr float GEAR_MAX_SPEED[] = { 11.5f, 19.0f, 28.0f, 37.5f, MAX_FORWARD_SPEED };
    constexpr float GEAR_ACCELERATION[] = { 18.5f, 14.0f, 11.0f, 8.5f, 6.5f };

    bool Contains(const std::string& value, const char* part) {
        return value.find(part) != std::string::npos;
    }

    float MoveTowards(float current, float target, float maxDelta) {
        if (current < target) return std::min(current + maxDelta, target);
        return std::max(current - maxDelta, target);
    }
}

bool VehicleController::Load(const std::string& path) {
    return model.LoadVisualData(path);
}

void VehicleController::UploadToGPU() {
    model.UploadToGPU();
}

bool VehicleController::InitAudio(ma_engine* engine) {
    ma_sound* sounds[] = { &engineIdle, &engineMid, &engineHigh, &skidSound };
    const char* paths[] = {
        "Sonidos/Vehiculo/R32_Idle.wav",
        "Sonidos/Vehiculo/R32_Mid.wav",
        "Sonidos/Vehiculo/R32_High.wav",
        "Sonidos/Vehiculo/R32_Derrape.wav"
    };

    bool allLoaded = true;
    for (int i = 0; i < 4; ++i) {
        audioReady[i] = ma_sound_init_from_file(engine, paths[i], MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, nullptr, sounds[i]) == MA_SUCCESS;
        allLoaded &= audioReady[i];
    }

    for (int i = 0; i < 4; ++i) {
        if (!audioReady[i]) continue;
        ma_sound_set_looping(sounds[i], MA_TRUE);
        ma_sound_set_volume(sounds[i], 0.0f);
        ma_sound_start(sounds[i]);
    }
    return allLoaded;
}

void VehicleController::Update(
    GLFWwindow* window,
    const GameplayGamepadInput& gamepad,
    float deltaTime,
    PhysicsWorld& city,
    glm::vec3& playerPosition,
    glm::vec3& cameraPosition,
    glm::vec3& cameraFront,
    float& cameraYaw,
    float& cameraPitch) {
    const bool interactDown = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS || gamepad.interactDown;
    if (interactDown && !interactWasDown) {
        if (driving) {
            driving = false;
            const glm::vec3 right(std::cos(yaw), 0.0f, -std::sin(yaw));
            playerPosition = position + right * 2.2f + glm::vec3(0.0f, 1.75f, 0.0f);
            cameraPosition = playerPosition;
        }
        else if (glm::distance(playerPosition, position) < 6.0f) {
            driving = true;
            const glm::vec3 forward(std::sin(yaw), 0.0f, std::cos(yaw));
            cameraYaw = glm::degrees(std::atan2(forward.z, forward.x));
            cameraPitch = -12.0f;
        }
    }
    interactWasDown = interactDown;

    if (!driving) {
        UpdateGroundPose(city, deltaTime);
        UpdateAudio(deltaTime, 0.0f, false);
        return;
    }

    float throttle = 0.0f;
    float steerInput = 0.0f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) throttle += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) throttle -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) steerInput += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) steerInput -= 1.0f;
    if (gamepad.connected) {
        throttle += gamepad.rightTrigger - gamepad.leftTrigger;
        steerInput -= gamepad.leftX;
        cameraYaw += gamepad.rightX * 145.0f * deltaTime;
        cameraPitch -= gamepad.rightY * 95.0f * deltaTime;
    }
    cameraPitch = std::clamp(cameraPitch, -38.0f, 16.0f);
    throttle = std::clamp(throttle, -1.0f, 1.0f);
    steerInput = std::clamp(steerInput, -1.0f, 1.0f);

    throttleSmoothed = MoveTowards(throttleSmoothed, throttle, deltaTime * 3.8f);
    const bool handbrake = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS || gamepad.handbrakeDown;
    const float forwardSpeed = std::max(speed, 0.0f);
    const float speedRatio = std::clamp(std::abs(speed) / MAX_FORWARD_SPEED, 0.0f, 1.0f);

    shiftCooldown = std::max(shiftCooldown - deltaTime, 0.0f);
    const int gearIndex = std::clamp(currentGear, 0, 4);
    const float gearRange = std::max(GEAR_MAX_SPEED[gearIndex] - GEAR_MIN_SPEED[gearIndex], 1.0f);
    float targetRpm = std::clamp(0.28f + ((forwardSpeed - GEAR_MIN_SPEED[gearIndex]) / gearRange) * 0.68f, IDLE_RPM, 1.0f);
    if (forwardSpeed < 1.0f) {
        targetRpm = IDLE_RPM + std::max(throttleSmoothed, 0.0f) * 0.24f;
    }

    if (currentGear < 4 && forwardSpeed > GEAR_MAX_SPEED[currentGear] && shiftCooldown <= 0.0f) {
        ++currentGear;
        shiftCooldown = 0.18f;
        targetRpm = 0.62f;
    }
    else if (currentGear > 0 && forwardSpeed < GEAR_MIN_SPEED[currentGear] - 1.5f) {
        --currentGear;
        targetRpm = 0.68f;
    }

    const float rpmResponse = shiftCooldown > 0.0f ? 8.0f : 5.0f;
    engineRpm += (targetRpm - engineRpm) * std::clamp(deltaTime * rpmResponse, 0.0f, 1.0f);

    if (throttleSmoothed > 0.02f) {
        if (speed < -0.4f) {
            speed = MoveTowards(speed, 0.0f, 28.0f * deltaTime);
        }
        else {
            const float torqueCurve = std::clamp(1.12f - std::pow((engineRpm - 0.62f) * 1.45f, 2.0f), 0.42f, 1.0f);
            const float clutch = shiftCooldown > 0.0f ? 0.82f : 1.0f;
            speed += throttleSmoothed * GEAR_ACCELERATION[currentGear] * torqueCurve * clutch * deltaTime;
        }
    }
    else if (throttleSmoothed < -0.02f) {
        if (speed > 0.5f) {
            speed = MoveTowards(speed, 0.0f, 30.0f * deltaTime);
        }
        else {
            speed -= std::abs(throttleSmoothed) * 8.5f * deltaTime;
        }
    }

    const float coastDrag = handbrake ? 0.935f : (std::abs(throttleSmoothed) < 0.02f ? 0.978f : 0.996f);
    speed *= std::pow(coastDrag, deltaTime * 60.0f);
    speed = std::clamp(speed, -MAX_REVERSE_SPEED, MAX_FORWARD_SPEED);

    const float steeringLimit = glm::mix(glm::radians(31.0f), glm::radians(11.0f), speedRatio);
    const float targetSteering = steerInput * steeringLimit;
    steering += (targetSteering - steering) * std::clamp(deltaTime * (handbrake ? 10.0f : 7.5f), 0.0f, 1.0f);
    const float turnAuthority = glm::smoothstep(0.0f, 4.0f, std::abs(speed)) * glm::mix(2.7f, 1.05f, speedRatio);
    const float tractionLoss = std::clamp((handbrake ? 0.75f : 0.0f) + std::abs(steerInput) * glm::smoothstep(0.55f, 1.0f, speedRatio) * 0.25f, 0.0f, 0.85f);
    const float targetYawVelocity = steering * turnAuthority * (speed >= 0.0f ? 1.0f : -1.0f) * (1.0f + tractionLoss * 1.25f);
    const float yawGrip = glm::mix(glm::mix(9.0f, 5.8f, speedRatio), 2.7f, tractionLoss);
    yawVelocity += (targetYawVelocity - yawVelocity) * std::clamp(deltaTime * yawGrip, 0.0f, 1.0f);
    yaw += yawVelocity * deltaTime;
    bodyRoll += ((-steerInput * speedRatio * (handbrake ? 0.075f : 0.040f)) - bodyRoll) * std::clamp(deltaTime * 6.0f, 0.0f, 1.0f);

    const glm::vec3 forward(std::sin(yaw), 0.0f, std::cos(yaw));
    const glm::vec3 movementDirection = speed >= 0.0f ? forward : -forward;
    float obstacleDistance = 0.0f;
    const glm::vec3 obstacleRayOrigin = position + glm::vec3(0.0f, 0.9f, 0.0f) + movementDirection * 1.15f;
    if (std::abs(speed) > 0.1f &&
        city.Raycast(obstacleRayOrigin, movementDirection, obstacleDistance) &&
        obstacleDistance < 1.65f) {
        speed = 0.0f;
    }
    else {
        position += forward * speed * deltaTime;
    }
    UpdateGroundPose(city, deltaTime);
    wheelSpin += speed * deltaTime / 0.33f;

    const float orbitYaw = glm::radians(cameraYaw);
    const float orbitPitch = glm::radians(std::clamp(cameraPitch, -42.0f, 18.0f));
    const glm::vec3 orbitDirection = glm::normalize(glm::vec3(
        std::cos(orbitYaw) * std::cos(orbitPitch),
        std::sin(orbitPitch),
        std::sin(orbitYaw) * std::cos(orbitPitch)));
    const float cameraPullback = glm::mix(8.6f, 11.3f, speedRatio);
    const glm::vec3 target = position + forward * glm::mix(0.0f, 2.4f, speedRatio) + glm::vec3(0.0f, 1.1f, 0.0f);
    glm::vec3 desiredCamera = target - orbitDirection * cameraPullback;
    glm::vec3 cameraRay = desiredCamera - target;
    float cameraObstacleDistance = 0.0f;
    if (glm::length(cameraRay) > 0.001f &&
        city.Raycast(target, glm::normalize(cameraRay), cameraObstacleDistance) &&
        cameraObstacleDistance < glm::length(cameraRay)) {
        desiredCamera = target + glm::normalize(cameraRay) * std::max(cameraObstacleDistance - 0.45f, 1.4f);
    }
    cameraPosition += (desiredCamera - cameraPosition) * std::clamp(deltaTime * glm::mix(24.0f, 16.0f, speedRatio), 0.0f, 1.0f);
    cameraFront = glm::normalize(target - cameraPosition);
    playerPosition = cameraPosition;
    UpdateAudio(deltaTime, steerInput, handbrake);
}

void VehicleController::UpdateAudio(float deltaTime, float steerInput, bool handbrake) {
    const float speedRatio = std::clamp(std::abs(speed) / MAX_FORWARD_SPEED, 0.0f, 1.0f);
    if (!driving) {
        engineRpm = MoveTowards(engineRpm, 0.0f, deltaTime * 2.0f);
    }

    const float idleWeight = 1.0f - glm::smoothstep(0.22f, 0.56f, engineRpm);
    const float midWeight = glm::smoothstep(0.18f, 0.52f, engineRpm) * (1.0f - glm::smoothstep(0.56f, 0.92f, engineRpm));
    const float highWeight = glm::smoothstep(0.52f, 0.92f, engineRpm);
    const float load = 0.62f + std::max(throttleSmoothed, 0.0f) * 0.25f;
    const float idleVolume = driving && !audioPaused ? idleWeight * 0.25f * load : 0.0f;
    const float midVolume = driving && !audioPaused ? midWeight * 0.25f * load : 0.0f;
    const float highVolume = driving && !audioPaused ? highWeight * 0.25f * load : 0.0f;

    if (audioReady[0]) {
        ma_sound_set_volume(&engineIdle, idleVolume);
        ma_sound_set_pitch(&engineIdle, 0.96f + engineRpm * 0.12f);
    }
    if (audioReady[1]) {
        ma_sound_set_volume(&engineMid, midVolume);
        ma_sound_set_pitch(&engineMid, 0.88f + engineRpm * 0.25f);
    }
    if (audioReady[2]) {
        ma_sound_set_volume(&engineHigh, highVolume);
        ma_sound_set_pitch(&engineHigh, 0.86f + engineRpm * 0.24f);
    }
    const float targetSkid = driving && !audioPaused
        ? std::clamp((handbrake ? 1.0f : 0.35f) * std::abs(steerInput) * glm::smoothstep(0.18f, 0.75f, speedRatio), 0.0f, 1.0f)
        : 0.0f;
    skidAmount += (targetSkid - skidAmount) * std::clamp(deltaTime * (targetSkid > skidAmount ? 8.0f : 4.0f), 0.0f, 1.0f);
    if (audioReady[3]) {
        ma_sound_set_volume(&skidSound, skidAmount * 0.38f);
        ma_sound_set_pitch(&skidSound, 0.88f + speedRatio * 0.20f);
    }
}

void VehicleController::UpdateGroundPose(PhysicsWorld& city, float deltaTime) {
    const glm::vec3 forward(std::sin(yaw), 0.0f, std::cos(yaw));
    const glm::vec3 right(std::cos(yaw), 0.0f, -std::sin(yaw));
    constexpr float halfWheelbase = 1.38f * VEHICLE_SCALE;
    constexpr float halfTrack = 0.78f * VEHICLE_SCALE;
    // The GLB origin already includes the tire radius; keep the visual tires in contact.
    constexpr float chassisClearance = 0.04f;

    float heights[4]{};
    bool grounded[4]{};
    const glm::vec3 offsets[4] = {
        forward * halfWheelbase - right * halfTrack,
        forward * halfWheelbase + right * halfTrack,
        -forward * halfWheelbase - right * halfTrack,
        -forward * halfWheelbase + right * halfTrack
    };

    int hits = 0;
    for (int i = 0; i < 4; ++i) {
        const glm::vec3 origin = position + offsets[i] + glm::vec3(0.0f, 4.0f, 0.0f);
        float distance = 0.0f;
        if (city.RaycastStatic(origin, glm::vec3(0.0f, -1.0f, 0.0f), distance) && distance < 8.0f) {
            heights[i] = origin.y - distance;
            grounded[i] = true;
            ++hits;
        }
    }

    if (hits < 3) {
        return;
    }

    float measuredAverage = 0.0f;
    for (int i = 0; i < 4; ++i) {
        if (grounded[i]) {
            measuredAverage += heights[i];
        }
    }
    measuredAverage /= static_cast<float>(hits);
    for (int i = 0; i < 4; ++i) {
        if (!grounded[i]) {
            heights[i] = measuredAverage;
        }
    }

    const float frontHeight = (heights[0] + heights[1]) * 0.5f;
    const float rearHeight = (heights[2] + heights[3]) * 0.5f;
    const float leftHeight = (heights[0] + heights[2]) * 0.5f;
    const float rightHeight = (heights[1] + heights[3]) * 0.5f;
    const float averageHeight = (frontHeight + rearHeight) * 0.5f;
    const float targetPitch = std::clamp(-std::atan2(frontHeight - rearHeight, halfWheelbase * 2.0f), -0.42f, 0.42f);
    const float targetRoll = std::clamp(std::atan2(rightHeight - leftHeight, halfTrack * 2.0f), -0.30f, 0.30f);
    const float targetHeight = averageHeight + chassisClearance;
    const float verticalResponse = targetHeight > position.y ? 32.0f : 28.0f;
    const float suspensionBlend = std::clamp(deltaTime * verticalResponse, 0.0f, 1.0f);
    const float rotationBlend = std::clamp(deltaTime * 14.0f, 0.0f, 1.0f);

    position.y += (targetHeight - position.y) * suspensionBlend;
    groundPitch += (targetPitch - groundPitch) * rotationBlend;
    groundRoll += (targetRoll - groundRoll) * rotationBlend;
}

void VehicleController::UploadHeadlights(Shader& shader, float nightFactor, float rainIntensity) const {
    const glm::mat4 bodyTransform = BuildBodyTransform();
    const glm::vec3 direction = glm::normalize(glm::vec3(bodyTransform * glm::vec4(0.0f, -0.10f, 1.0f, 0.0f)));
    const float intensity = std::clamp(nightFactor * 1.32f + rainIntensity * 0.48f, 0.0f, 1.0f);

    shader.setFloat("headlightIntensity", intensity);
    shader.setVec3("headlights[0].position", glm::vec3(bodyTransform * glm::vec4(-0.48f, 0.34f, 1.18f, 1.0f)));
    shader.setVec3("headlights[1].position", glm::vec3(bodyTransform * glm::vec4(0.48f, 0.34f, 1.18f, 1.0f)));
    shader.setVec3("headlights[0].direction", direction);
    shader.setVec3("headlights[1].direction", direction);
}

glm::mat4 VehicleController::BuildBodyTransform() const {
    glm::mat4 transform(1.0f);
    transform = glm::translate(transform, position);
    transform = glm::rotate(transform, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    transform = glm::rotate(transform, groundPitch, glm::vec3(1.0f, 0.0f, 0.0f));
    transform = glm::rotate(transform, groundRoll + bodyRoll, glm::vec3(0.0f, 0.0f, 1.0f));
    transform = glm::scale(transform, glm::vec3(VEHICLE_SCALE));
    return transform;
}

glm::mat4 VehicleController::BuildMeshTransform(const RenderMesh& mesh) const {
    glm::mat4 transform = BuildBodyTransform();
    const bool frontWheel = Contains(mesh.nodeName, "_FL") || Contains(mesh.nodeName, "_FR");
    const bool rotatingWheel = Contains(mesh.nodeName, "Tire_") || Contains(mesh.nodeName, "Rim_") || Contains(mesh.nodeName, "Disk_");
    if (!frontWheel && !rotatingWheel) {
        return transform;
    }

    transform = glm::translate(transform, mesh.pivot);
    if (frontWheel) {
        transform = glm::rotate(transform, steering, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    if (rotatingWheel) {
        transform = glm::rotate(transform, wheelSpin, glm::vec3(1.0f, 0.0f, 0.0f));
    }
    return glm::translate(transform, -mesh.pivot);
}

void VehicleController::Draw(Shader& shader) const {
    shader.setFloat("vehicleSurface", 1.0f);
    for (const RenderMesh& mesh : model.visualMeshes) {
        shader.setMat4("model", BuildMeshTransform(mesh));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mesh.textureID);
        glBindVertexArray(mesh.VAO);
        glDrawElements(GL_TRIANGLES, mesh.numIndices, GL_UNSIGNED_INT, nullptr);
    }
    shader.setFloat("vehicleSurface", 0.0f);
    shader.setMat4("model", glm::mat4(1.0f));
}

void VehicleController::DrawReplica(Shader& shader, const glm::vec3& replicaPosition, float replicaYaw, float replicaPitch, float replicaWheelSpin, const glm::vec3& color) const {
    glm::mat4 bodyTransform(1.0f);
    bodyTransform = glm::translate(bodyTransform, replicaPosition);
    bodyTransform = glm::rotate(bodyTransform, replicaYaw, glm::vec3(0.0f, 1.0f, 0.0f));
    bodyTransform = glm::rotate(bodyTransform, replicaPitch, glm::vec3(1.0f, 0.0f, 0.0f));
    bodyTransform = glm::scale(bodyTransform, glm::vec3(VEHICLE_SCALE));

    shader.setVec3("objectTint", color);
    shader.setFloat("vehicleSurface", 1.0f);
    for (const RenderMesh& mesh : model.visualMeshes) {
        glm::mat4 transform = bodyTransform;
        const bool rotatingWheel = Contains(mesh.nodeName, "Tire_") || Contains(mesh.nodeName, "Rim_") || Contains(mesh.nodeName, "Disk_");
        if (rotatingWheel) {
            transform = glm::translate(transform, mesh.pivot);
            transform = glm::rotate(transform, replicaWheelSpin, glm::vec3(1.0f, 0.0f, 0.0f));
            transform = glm::translate(transform, -mesh.pivot);
        }

        shader.setMat4("model", transform);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mesh.textureID);
        glBindVertexArray(mesh.VAO);
        glDrawElements(GL_TRIANGLES, mesh.numIndices, GL_UNSIGNED_INT, nullptr);
    }
    shader.setFloat("vehicleSurface", 0.0f);
    shader.setVec3("objectTint", glm::vec3(1.0f));
    shader.setMat4("model", glm::mat4(1.0f));
}

void VehicleController::Shutdown() {
    ma_sound* sounds[] = { &engineIdle, &engineMid, &engineHigh, &skidSound };
    for (int i = 0; i < 4; ++i) {
        if (audioReady[i]) {
            ma_sound_uninit(sounds[i]);
            audioReady[i] = false;
        }
    }

    for (const RenderMesh& mesh : model.visualMeshes) {
        glDeleteVertexArrays(1, &mesh.VAO);
        glDeleteBuffers(1, &mesh.VBO_Pos);
        glDeleteBuffers(1, &mesh.VBO_UV);
        glDeleteBuffers(1, &mesh.VBO_Normal);
        glDeleteBuffers(1, &mesh.EBO);
    }
}

void VehicleController::SetAudioPaused(bool paused) {
    audioPaused = paused;
    if (!paused) {
        return;
    }

    ma_sound* sounds[] = { &engineIdle, &engineMid, &engineHigh, &skidSound };
    for (int i = 0; i < 4; ++i) {
        if (audioReady[i]) {
            ma_sound_set_volume(sounds[i], 0.0f);
        }
    }
}

void VehicleController::ResetForMainMenu() {
    driving = false;
    speed = 0.0f;
    steering = 0.0f;
    throttleSmoothed = 0.0f;
    yawVelocity = 0.0f;
    engineRpm = 0.0f;
    skidAmount = 0.0f;
    SetAudioPaused(true);
}

bool VehicleController::IsDriving() const {
    return driving;
}

const glm::vec3& VehicleController::GetPosition() const {
    return position;
}
