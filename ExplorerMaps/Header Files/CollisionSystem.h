#pragma once

#include <vector>

#include "ColliderTypes.h"

class CollisionSystem
{
public:
    bool Raycast(
        const std::vector<BoxCollider>& colliders,
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDirection,
        int& outColliderIndex,
        float& outDistance) const;

    bool HasColliderIntersection(const std::vector<BoxCollider>& colliders, int index) const;
    bool IntersectsCamera(const BoxCollider& collider, const glm::vec3& cameraPosition, float radius) const;
    bool IsCameraColliding(const std::vector<BoxCollider>& colliders, const glm::vec3& cameraPosition, float radius) const;
    glm::vec3 ResolveCameraPosition(
        const std::vector<BoxCollider>& colliders,
        const glm::vec3& previousPosition,
        const glm::vec3& desiredPosition,
        float radius) const;

private:
    bool Intersects(const BoxCollider& a, const BoxCollider& b) const;
};

