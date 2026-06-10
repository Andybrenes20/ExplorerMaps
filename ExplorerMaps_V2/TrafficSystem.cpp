#include "TrafficSystem.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>

#include "PhysicsWorld.h"
#include "Shader.h"
#include "VehicleController.h"

namespace {
    constexpr float kZoneRadius = 58.0f;
    constexpr float kRecycleDistance = 110.0f;
    constexpr float kDrawDistance = 180.0f;
    constexpr float kWheelRadius = 0.33f;
    constexpr float kGroundOffset = 0.13f;
    constexpr float kPlayerHeight = 1.75f;
    constexpr float kRespawnDelay = 0.75f;
    constexpr float kFollowingDistance = 14.0f;

    glm::vec3 SnapToStreetAxis(const glm::vec3& direction) {
        return std::abs(direction.x) > std::abs(direction.z)
            ? glm::vec3(direction.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f)
            : glm::vec3(0.0f, 0.0f, direction.z >= 0.0f ? 1.0f : -1.0f);
    }
}

void TrafficSystem::Initialize() {
    cars.resize(5);
    cars[0].color = glm::vec3(0.95f, 0.22f, 0.18f);
    cars[1].color = glm::vec3(0.20f, 0.48f, 0.95f);
    cars[2].color = glm::vec3(0.92f, 0.76f, 0.18f);
    cars[3].color = glm::vec3(0.20f, 0.78f, 0.42f);
    cars[4].color = glm::vec3(0.82f, 0.30f, 0.88f);

    for (std::size_t i = 0; i < cars.size(); ++i) {
        TrafficCar& car = cars[i];
        car.directionSign = i < 3 ? 1.0f : -1.0f;
        car.laneSide = car.directionSign;
        car.cruiseSpeed = 8.5f + static_cast<float>(i % 3) * 0.75f;
        car.speed = car.cruiseSpeed;
    }
}

void TrafficSystem::Regenerate(PhysicsWorld& city, const glm::vec3& userPosition, const glm::vec3& userForward) {
    const glm::vec3 zoneMovement = userPosition - activeZoneCenter;
    const glm::vec3 desiredDirection = zoneReady && glm::length(glm::vec2(zoneMovement.x, zoneMovement.z)) > 1.0f
        ? zoneMovement
        : userForward;
    travelDirection = SnapToStreetAxis(desiredDirection);
    activeZoneCenter = userPosition;

    const float distances[] = { 48.0f, 68.0f, 88.0f, 58.0f, 82.0f };
    for (std::size_t i = 0; i < cars.size(); ++i) {
        TrafficCar& car = cars[i];
        car.speed = car.cruiseSpeed;
        car.groundPitch = 0.0f;
        car.groundCheckTimer = 0.0f;
        car.obstacleCheckTimer = 0.0f;
        car.respawnTimer = 0.0f;
        car.respawnDistance = distances[i];
        car.active = FindLaneSpawn(car, city, userPosition, userForward, car.laneSide, car.directionSign, distances[i]);
        if (!car.active) {
            car.respawnTimer = kRespawnDelay + static_cast<float>(i) * 0.45f;
        }
    }
    zoneReady = true;
}

bool TrafficSystem::FindLaneSpawn(
    TrafficCar& car,
    PhysicsWorld& city,
    const glm::vec3& userPosition,
    const glm::vec3& userForward,
    float laneSide,
    float directionSign,
    float distance) {
    const glm::vec3 right(travelDirection.z, 0.0f, -travelDirection.x);
    const glm::vec3 flatView = glm::length(glm::vec2(userForward.x, userForward.z)) > 0.001f
        ? glm::normalize(glm::vec3(userForward.x, 0.0f, userForward.z))
        : travelDirection;
    const float longitudinalOffsets[] = { -distance, -distance - 18.0f, distance, distance + 18.0f };
    const float laneOffsets[] = { laneSide * 3.2f, laneSide * 5.8f, -laneSide * 3.2f };

    for (float longitudinal : longitudinalOffsets) {
        for (float lateral : laneOffsets) {
            const glm::vec3 candidate = userPosition + travelDirection * longitudinal + right * lateral;
            const glm::vec3 toCandidate = glm::normalize(glm::vec3(candidate.x - userPosition.x, 0.0f, candidate.z - userPosition.z));
            if (glm::dot(toCandidate, flatView) > 0.18f) {
                continue;
            }

            car.position = candidate;
            const glm::vec3 carDirection = travelDirection * directionSign;
            car.yaw = std::atan2(carDirection.x, carDirection.z);
            bool overlapsTraffic = false;
            for (const TrafficCar& other : cars) {
                if (&other != &car && other.active && glm::distance(glm::vec2(other.position.x, other.position.z), glm::vec2(candidate.x, candidate.z)) < 18.0f) {
                    overlapsTraffic = true;
                    break;
                }
            }
            if (overlapsTraffic) {
                continue;
            }
            if (PlaceOnGround(car, city, true) &&
                std::abs(car.position.y - (userPosition.y - kPlayerHeight + kGroundOffset)) < 3.0f &&
                IsLaneClear(car, city)) {
                return true;
            }
        }
    }
    return false;
}

bool TrafficSystem::IsLaneClear(const TrafficCar& car, PhysicsWorld& city) const {
    const glm::vec3 forward(std::sin(car.yaw), 0.0f, std::cos(car.yaw));
    for (float distance : { -8.0f, 8.0f, 16.0f }) {
        const glm::vec3 origin = car.position + forward * distance + glm::vec3(0.0f, 5.0f, 0.0f);
        float groundDistance = 0.0f;
        if (!city.Raycast(origin, glm::vec3(0.0f, -1.0f, 0.0f), groundDistance) || groundDistance > 12.0f) {
            return false;
        }
        const float groundY = origin.y - groundDistance + kGroundOffset;
        if (std::abs(groundY - car.position.y) > 1.2f) {
            return false;
        }
    }

    float obstacleDistance = 0.0f;
    return !city.Raycast(car.position + glm::vec3(0.0f, 0.8f, 0.0f), forward, obstacleDistance) ||
        obstacleDistance > 14.0f;
}

bool TrafficSystem::PlaceOnGround(TrafficCar& car, PhysicsWorld& city, bool immediate) {
    const glm::vec3 forward(std::sin(car.yaw), 0.0f, std::cos(car.yaw));
    float centerDistance = 0.0f;
    float frontDistance = 0.0f;
    const glm::vec3 centerOrigin = car.position + glm::vec3(0.0f, 5.0f, 0.0f);
    const glm::vec3 frontOrigin = centerOrigin + forward * 1.35f;
    if (!city.Raycast(centerOrigin, glm::vec3(0.0f, -1.0f, 0.0f), centerDistance) || centerDistance > 12.0f ||
        !city.Raycast(frontOrigin, glm::vec3(0.0f, -1.0f, 0.0f), frontDistance) || frontDistance > 12.0f) {
        return false;
    }

    const float centerY = centerOrigin.y - centerDistance + kGroundOffset;
    const float frontY = frontOrigin.y - frontDistance + kGroundOffset;
    const float targetPitch = -std::atan2(frontY - centerY, 1.35f);
    const float blend = immediate ? 1.0f : 0.45f;
    car.position.y += (centerY - car.position.y) * blend;
    car.groundPitch += (targetPitch - car.groundPitch) * blend;
    return true;
}

void TrafficSystem::Update(float deltaTime, PhysicsWorld& city, const glm::vec3& userPosition, const glm::vec3& userForward) {
    const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 0.05f);
    if (!zoneReady || glm::distance(glm::vec2(userPosition.x, userPosition.z), glm::vec2(activeZoneCenter.x, activeZoneCenter.z)) > kZoneRadius) {
        Regenerate(city, userPosition, userForward);
    }
    for (TrafficCar& car : cars) {
        if (!car.active) {
            car.respawnTimer -= safeDeltaTime;
            if (car.respawnTimer <= 0.0f) {
                const std::size_t carIndex = static_cast<std::size_t>(&car - cars.data());
                car.active = FindLaneSpawn(car, city, userPosition, userForward, car.laneSide, car.directionSign, car.respawnDistance);
                if (car.active) {
                    car.speed = car.cruiseSpeed;
                    car.groundCheckTimer = 0.0f;
                    car.obstacleCheckTimer = 0.0f;
                    car.respawnDistance = 72.0f;
                }
                else {
                    car.respawnDistance = std::min(car.respawnDistance + 18.0f, 126.0f);
                    car.respawnTimer = kRespawnDelay + static_cast<float>(carIndex) * 0.35f;
                }
            }
            continue;
        }
        const glm::vec3 forward(std::sin(car.yaw), 0.0f, std::cos(car.yaw));
        float targetSpeed = car.cruiseSpeed;
        for (const TrafficCar& other : cars) {
            if (&other == &car || !other.active || other.directionSign != car.directionSign) {
                continue;
            }
            const glm::vec3 separation = other.position - car.position;
            const float forwardDistance = glm::dot(separation, forward);
            const float sideDistance = std::abs(glm::dot(separation, glm::vec3(forward.z, 0.0f, -forward.x)));
            if (forwardDistance > 0.0f && forwardDistance < kFollowingDistance && sideDistance < 4.5f) {
                targetSpeed = std::min(targetSpeed, other.speed * (forwardDistance / kFollowingDistance));
            }
        }
        car.speed += (targetSpeed - car.speed) * std::clamp(safeDeltaTime * 3.5f, 0.0f, 1.0f);
        car.position += forward * car.speed * safeDeltaTime;
        car.wheelSpin += car.speed * safeDeltaTime / kWheelRadius;
        car.groundCheckTimer -= safeDeltaTime;
        car.obstacleCheckTimer -= safeDeltaTime;

        bool recycle = glm::distance(glm::vec2(car.position.x, car.position.z), glm::vec2(userPosition.x, userPosition.z)) > kRecycleDistance;
        if (!recycle && car.groundCheckTimer <= 0.0f) {
            recycle = !PlaceOnGround(car, city, false);
            car.groundCheckTimer = 0.18f;
        }
        if (!recycle && car.obstacleCheckTimer <= 0.0f) {
            float obstacleDistance = 0.0f;
            recycle = city.Raycast(car.position + glm::vec3(0.0f, 0.8f, 0.0f) + forward * 1.4f, forward, obstacleDistance) &&
                obstacleDistance < 4.0f;
            car.obstacleCheckTimer = 0.35f;
        }
        if (recycle) {
            car.active = FindLaneSpawn(car, city, userPosition, userForward, car.laneSide, car.directionSign, 82.0f);
            if (!car.active) {
                car.respawnDistance = 82.0f;
                car.respawnTimer = kRespawnDelay;
            }
        }
    }
}

void TrafficSystem::Draw(Shader& shader, const VehicleController& vehicleModel, const glm::vec3& cameraPosition) const {
    const float maxDistanceSquared = kDrawDistance * kDrawDistance;
    for (const TrafficCar& car : cars) {
        if (!car.active) {
            continue;
        }
        const glm::vec3 offset = car.position - cameraPosition;
        if (glm::dot(offset, offset) > maxDistanceSquared) {
            continue;
        }

        vehicleModel.DrawReplica(shader, car.position, car.yaw, car.groundPitch, car.wheelSpin, car.color);
    }
}
