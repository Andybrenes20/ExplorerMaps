#include "TrafficSystem.h"

#include <algorithm>
#include <cmath>

#include "PhysicsWorld.h"
#include "Shader.h"
#include "VehicleController.h"

namespace {
    constexpr float kZoneRadius = 58.0f;
    constexpr float kRecycleDistance = 110.0f;
    constexpr float kDrawDistance = 180.0f;
    constexpr float kWheelRadius = 0.33f;
    constexpr float kGroundOffset = 0.13f;
    constexpr float kRespawnDelay = 0.75f;
    constexpr float kFollowingDistance = 14.0f;

    struct TrafficRoute {
        glm::vec3 start;
        glm::vec3 end;
        float laneOffset;
    };
    //Rutas de Trafico
    const TrafficRoute kTrafficRoutes[] = {
        { glm::vec3(424.5f, 37.8f, 161.3f), glm::vec3(-171.8f, 37.8f, 158.5f), -3.2f },
        { glm::vec3(309.7f, 37.8f, 135.5f), glm::vec3(308.8f, 27.0f, -37.8f), -3.2f },
        { glm::vec3(286.7f, 52.8f, 206.8f), glm::vec3(-109.9f, 52.8f, 207.0f), -12.0f },
        { glm::vec3(-131.6f, 52.8f, 211.0f), glm::vec3(278.5f, 52.8f, 212.1f), -1.0f },
        { glm::vec3(-139.4f, 37.8f, 141.2f), glm::vec3(277.9f, 37.8f, 140.0f), -4.5f },
        { glm::vec3(-270.7f, 37.8f, -96.2f), glm::vec3(-961.2f, 37.8f, -216.4f), 9.0f },
        { glm::vec3(-976.3f, 37.8f, -199.4f), glm::vec3(-282.4f, 37.8f, -79.0f), 2.0f },

    };
    constexpr std::size_t kTrafficRouteCount = sizeof(kTrafficRoutes) / sizeof(kTrafficRoutes[0]);
    constexpr std::size_t kCarsPerRoute = 5;
    constexpr float kRoutePadding = 10.0f;

    glm::vec3 RouteDelta(const TrafficRoute& route) {
        return glm::vec3(route.end.x - route.start.x, 0.0f, route.end.z - route.start.z);
    }

    float RouteLength(const TrafficRoute& route) {
        return glm::length(RouteDelta(route));
    }

    glm::vec3 RouteDirection(const TrafficRoute& route) {
        const glm::vec3 delta = RouteDelta(route);
        const float length = glm::length(delta);
        return length > 0.001f ? delta / length : glm::vec3(-1.0f, 0.0f, 0.0f);
    }

    float ProjectOnRoute(const glm::vec3& position, const TrafficRoute& route) {
        return glm::dot(glm::vec3(position.x - route.start.x, 0.0f, position.z - route.start.z), RouteDirection(route));
    }

    glm::vec3 RoutePointAt(float distance, const TrafficRoute& route) {
        const float routeLength = RouteLength(route);
        const float clampedDistance = (std::clamp)(distance, kRoutePadding, routeLength - kRoutePadding);
        const float routeAmount = routeLength > 0.001f ? clampedDistance / routeLength : 0.0f;
        return route.start + (route.end - route.start) * routeAmount;
    }
}

void TrafficSystem::Initialize() {
    const glm::vec3 colors[] = {
        glm::vec3(0.95f, 0.22f, 0.18f),
        glm::vec3(0.20f, 0.48f, 0.95f),
        glm::vec3(0.92f, 0.76f, 0.18f),
        glm::vec3(0.20f, 0.78f, 0.42f),
        glm::vec3(0.82f, 0.30f, 0.88f),
    };

    cars.resize(kTrafficRouteCount * kCarsPerRoute);

    for (std::size_t i = 0; i < cars.size(); ++i) {
        TrafficCar& car = cars[i];
        car.color = colors[i % 5];
        car.directionSign = 1.0f;
        car.routeIndex = (std::min)(i / kCarsPerRoute, kTrafficRouteCount - 1);
        car.cruiseSpeed = 8.5f + static_cast<float>(i % 3) * 0.75f;
        car.speed = car.cruiseSpeed;
    }
}

void TrafficSystem::Regenerate(PhysicsWorld& city, const glm::vec3& userPosition, const glm::vec3& userForward) {
    activeZoneCenter = userPosition;

    const float distances[] = { 48.0f, 68.0f, 88.0f, 58.0f, 82.0f };
    for (std::size_t i = 0; i < cars.size(); ++i) {
        TrafficCar& car = cars[i];
        car.speed = car.cruiseSpeed;
        car.groundPitch = 0.0f;
        car.groundCheckTimer = 0.0f;
        car.obstacleCheckTimer = 0.0f;
        car.respawnTimer = 0.0f;
        car.respawnDistance = distances[i % 5];
        car.active = FindLaneSpawn(car, city, userPosition, userForward, car.directionSign, distances[i % 5]);
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
    float directionSign,
    float distance) {
    const TrafficRoute& route = kTrafficRoutes[car.routeIndex];
    const glm::vec3 routeDirection = RouteDirection(route);
    const glm::vec3 right(routeDirection.z, 0.0f, -routeDirection.x);
    const float routeLength = RouteLength(route);
    const float anchorDistance = (std::clamp)(ProjectOnRoute(userPosition, route), kRoutePadding, routeLength - kRoutePadding);
    const bool keepBehindCamera = car.routeIndex == 0;
    const glm::vec3 flatView = glm::length(glm::vec2(userForward.x, userForward.z)) > 0.001f
        ? glm::normalize(glm::vec3(userForward.x, 0.0f, userForward.z))
        : routeDirection;
    const float longitudinalOffsets[] = { -distance, -distance - 18.0f, distance, distance + 18.0f };
    const float laneOffsets[] = { route.laneOffset };

    for (float longitudinal : longitudinalOffsets) {
        const float candidateDistance = anchorDistance + longitudinal;
        if (candidateDistance < kRoutePadding || candidateDistance > routeLength - kRoutePadding) {
            continue;
        }

        for (float lateral : laneOffsets) {
            const glm::vec3 routeCenter = RoutePointAt(candidateDistance, route);
            const glm::vec3 candidate = routeCenter + right * lateral;
            const glm::vec3 toCandidateFlat(candidate.x - userPosition.x, 0.0f, candidate.z - userPosition.z);
            if (keepBehindCamera &&
                glm::length(glm::vec2(toCandidateFlat.x, toCandidateFlat.z)) > 0.001f &&
                glm::dot(glm::normalize(toCandidateFlat), flatView) > 0.18f) {
                continue;
            }

            car.position = candidate;
            const glm::vec3 carDirection = routeDirection * directionSign;
            car.yaw = std::atan2(carDirection.x, carDirection.z);
            bool overlapsTraffic = false;
            for (const TrafficCar& other : cars) {
                if (&other != &car && other.active && other.routeIndex == car.routeIndex &&
                    glm::distance(glm::vec2(other.position.x, other.position.z), glm::vec2(candidate.x, candidate.z)) < 18.0f) {
                    overlapsTraffic = true;
                    break;
                }
            }
            if (overlapsTraffic) {
                continue;
            }
            if (!PlaceOnGround(car, city, true)) {
                continue;
            }

            const bool heightMatchesRoute = car.routeIndex != 0 ||
                std::abs(car.position.y - (routeCenter.y + kGroundOffset)) < 3.0f;
            if (heightMatchesRoute && IsLaneClear(car, city)) {
                return true;
            }
        }
    }
    return false;
}

bool TrafficSystem::IsLaneClear(const TrafficCar& car, PhysicsWorld& city) const {
    const glm::vec3 forward(std::sin(car.yaw), 0.0f, std::cos(car.yaw));
    constexpr float sampleDistances[] = { -8.0f, 8.0f, 16.0f };
    for (float distance : sampleDistances) {
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
    const float safeDeltaTime = (std::clamp)(deltaTime, 0.0f, 0.05f);
    if (!zoneReady || glm::distance(glm::vec2(userPosition.x, userPosition.z), glm::vec2(activeZoneCenter.x, activeZoneCenter.z)) > kZoneRadius) {
        Regenerate(city, userPosition, userForward);
    }
    for (TrafficCar& car : cars) {
        if (!car.active) {
            car.respawnTimer -= safeDeltaTime;
            if (car.respawnTimer <= 0.0f) {
                const std::size_t carIndex = static_cast<std::size_t>(&car - cars.data());
                car.active = FindLaneSpawn(car, city, userPosition, userForward, car.directionSign, car.respawnDistance);
                if (car.active) {
                    car.speed = car.cruiseSpeed;
                    car.groundCheckTimer = 0.0f;
                    car.obstacleCheckTimer = 0.0f;
                    car.respawnDistance = 72.0f;
                }
                else {
                    car.respawnDistance = (std::min)(car.respawnDistance + 18.0f, 126.0f);
                    car.respawnTimer = kRespawnDelay + static_cast<float>(carIndex) * 0.35f;
                }
            }
            continue;
        }
        const glm::vec3 forward(std::sin(car.yaw), 0.0f, std::cos(car.yaw));
        float targetSpeed = car.cruiseSpeed;
        for (const TrafficCar& other : cars) {
            if (&other == &car || !other.active || other.routeIndex != car.routeIndex || other.directionSign != car.directionSign) {
                continue;
            }
            const glm::vec3 separation = other.position - car.position;
            const float forwardDistance = glm::dot(separation, forward);
            const float sideDistance = std::abs(glm::dot(separation, glm::vec3(forward.z, 0.0f, -forward.x)));
            if (forwardDistance > 0.0f && forwardDistance < kFollowingDistance && sideDistance < 4.5f) {
                targetSpeed = (std::min)(targetSpeed, other.speed * (forwardDistance / kFollowingDistance));
            }
        }
        car.speed += (targetSpeed - car.speed) * (std::clamp)(safeDeltaTime * 3.5f, 0.0f, 1.0f);
        car.position += forward * car.speed * safeDeltaTime;
        car.wheelSpin += car.speed * safeDeltaTime / kWheelRadius;
        car.groundCheckTimer -= safeDeltaTime;
        car.obstacleCheckTimer -= safeDeltaTime;

        const TrafficRoute& route = kTrafficRoutes[car.routeIndex];
        const float routeDistance = ProjectOnRoute(car.position, route);
        bool recycle = routeDistance < -kRoutePadding || routeDistance > RouteLength(route) + kRoutePadding ||
            glm::distance(glm::vec2(car.position.x, car.position.z), glm::vec2(userPosition.x, userPosition.z)) > kRecycleDistance;
        if (!recycle && car.groundCheckTimer <= 0.0f) {
            recycle = !PlaceOnGround(car, city, false);
            car.groundCheckTimer = 0.18f;
        }
        if (!recycle && car.obstacleCheckTimer <= 0.0f) {
            float obstacleDistance = 4.0f;
            recycle = city.Raycast(car.position + glm::vec3(0.0f, 0.8f, 0.0f) + forward * 1.4f, forward, obstacleDistance) &&
                obstacleDistance < 4.0f;
            car.obstacleCheckTimer = 0.35f;
        }
        if (recycle) {
            car.active = FindLaneSpawn(car, city, userPosition, userForward, car.directionSign, 82.0f);
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
