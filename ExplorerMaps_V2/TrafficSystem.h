#pragma once

#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

class PhysicsWorld;
class Shader;
class VehicleController;

class TrafficSystem {
public:
    void Initialize();
    void Update(float deltaTime, PhysicsWorld& city, const glm::vec3& userPosition, const glm::vec3& userForward);
    void Draw(Shader& shader, const VehicleController& vehicleModel, const glm::vec3& cameraPosition) const;

private:
    struct TrafficCar {
        glm::vec3 position = glm::vec3(0.0f);
        float speed = 0.0f;
        float wheelSpin = 0.0f;
        float yaw = 0.0f;
        float groundPitch = 0.0f;
        float groundCheckTimer = 0.0f;
        float obstacleCheckTimer = 0.0f;
        float respawnTimer = 0.0f;
        float respawnDistance = 72.0f;
        float directionSign = 1.0f;
        float cruiseSpeed = 9.0f;
        std::size_t routeIndex = 0;
        glm::vec3 color = glm::vec3(1.0f);
        bool active = false;
    };

    std::vector<TrafficCar> cars;
    glm::vec3 activeZoneCenter = glm::vec3(0.0f);
    bool zoneReady = false;

    void Regenerate(PhysicsWorld& city, const glm::vec3& userPosition, const glm::vec3& userForward);
    bool PlaceOnGround(TrafficCar& car, PhysicsWorld& city, bool immediate);
    bool FindLaneSpawn(TrafficCar& car, PhysicsWorld& city, const glm::vec3& userPosition, const glm::vec3& userForward, float directionSign, float distance);
    bool IsLaneClear(const TrafficCar& car, PhysicsWorld& city) const;
    void UpdateCollisionBoxes(PhysicsWorld& city) const;
};
