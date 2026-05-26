#include "CollisionSystem.h"

#include <cfloat>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace
{
    constexpr float kEpsilon = 1e-4f;

    struct OrientedBox
    {
        glm::vec3 center = glm::vec3(0.0f);
        glm::vec3 halfExtents = glm::vec3(0.5f);
        glm::vec3 axis[3] =
        {
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f)
        };
    };

    OrientedBox BuildOrientedBox(const BoxCollider& collider)
    {
        glm::mat4 rotationMatrix(1.0f);
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(collider.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(collider.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(collider.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

        OrientedBox box;
        box.center = collider.position;
        box.halfExtents = ColliderHalfExtents(collider);
        box.axis[0] = glm::normalize(glm::vec3(rotationMatrix[0]));
        box.axis[1] = glm::normalize(glm::vec3(rotationMatrix[1]));
        box.axis[2] = glm::normalize(glm::vec3(rotationMatrix[2]));
        return box;
    }

    glm::vec3 WorldToLocalPoint(const OrientedBox& box, const glm::vec3& point)
    {
        const glm::vec3 offset = point - box.center;
        return glm::vec3(
            glm::dot(offset, box.axis[0]),
            glm::dot(offset, box.axis[1]),
            glm::dot(offset, box.axis[2]));
    }

    glm::vec3 WorldToLocalDirection(const OrientedBox& box, const glm::vec3& direction)
    {
        return glm::vec3(
            glm::dot(direction, box.axis[0]),
            glm::dot(direction, box.axis[1]),
            glm::dot(direction, box.axis[2]));
    }

    glm::vec3 ClosestPointOnBox(const OrientedBox& box, const glm::vec3& point)
    {
        const glm::vec3 localPoint = WorldToLocalPoint(box, point);
        const glm::vec3 localClamped = glm::clamp(localPoint, -box.halfExtents, box.halfExtents);
        return box.center
            + box.axis[0] * localClamped.x
            + box.axis[1] * localClamped.y
            + box.axis[2] * localClamped.z;
    }

    bool IntersectsSphere(const OrientedBox& box, const glm::vec3& sphereCenter, float radius)
    {
        const glm::vec3 closestPoint = ClosestPointOnBox(box, sphereCenter);
        const glm::vec3 delta = closestPoint - sphereCenter;
        return glm::dot(delta, delta) <= (radius * radius);
    }

    bool IntersectsRay(const OrientedBox& box, const glm::vec3& rayOrigin, const glm::vec3& rayDirection, float& outDistance)
    {
        const glm::vec3 localOrigin = WorldToLocalPoint(box, rayOrigin);
        const glm::vec3 localDirection = WorldToLocalDirection(box, rayDirection);

        float tMin = 0.0f;
        float tMax = FLT_MAX;

        for (int axis = 0; axis < 3; ++axis)
        {
            if (std::abs(localDirection[axis]) < kEpsilon)
            {
                if (localOrigin[axis] < -box.halfExtents[axis] || localOrigin[axis] > box.halfExtents[axis])
                {
                    return false;
                }

                continue;
            }

            const float invDirection = 1.0f / localDirection[axis];
            float t0 = (-box.halfExtents[axis] - localOrigin[axis]) * invDirection;
            float t1 = ( box.halfExtents[axis] - localOrigin[axis]) * invDirection;
            if (t0 > t1)
            {
                std::swap(t0, t1);
            }

            tMin = std::max(tMin, t0);
            tMax = std::min(tMax, t1);
            if (tMin > tMax)
            {
                return false;
            }
        }

        outDistance = tMin;
        return true;
    }
}

bool CollisionSystem::Raycast(
    const std::vector<BoxCollider>& colliders,
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDirection,
    int& outColliderIndex,
    float& outDistance) const
{
    outColliderIndex = -1;
    outDistance = FLT_MAX;

    for (int i = 0; i < static_cast<int>(colliders.size()); ++i)
    {
        if (!colliders[i].visible)
        {
            continue;
        }

        float distance = FLT_MAX;
        if (!IntersectsRay(BuildOrientedBox(colliders[i]), rayOrigin, rayDirection, distance))
        {
            continue;
        }

        if (distance < outDistance)
        {
            outDistance = distance;
            outColliderIndex = i;
        }
    }

    return outColliderIndex >= 0;
}

bool CollisionSystem::HasColliderIntersection(const std::vector<BoxCollider>& colliders, int index) const
{
    if (index < 0 || index >= static_cast<int>(colliders.size()))
    {
        return false;
    }

    for (int i = 0; i < static_cast<int>(colliders.size()); ++i)
    {
        if (i == index)
        {
            continue;
        }

        if (Intersects(colliders[index], colliders[i]))
        {
            return true;
        }
    }

    return false;
}

bool CollisionSystem::IntersectsCamera(const BoxCollider& collider, const glm::vec3& cameraPosition, float radius) const
{
    return IntersectsSphere(BuildOrientedBox(collider), cameraPosition, radius);
}

bool CollisionSystem::IsCameraColliding(const std::vector<BoxCollider>& colliders, const glm::vec3& cameraPosition, float radius) const
{
    for (const BoxCollider& collider : colliders)
    {
        if (!collider.blocking)
        {
            continue;
        }

        if (IntersectsCamera(collider, cameraPosition, radius))
        {
            return true;
        }
    }

    return false;
}

glm::vec3 CollisionSystem::ResolveCameraPosition(
    const std::vector<BoxCollider>& colliders,
    const glm::vec3& previousPosition,
    const glm::vec3& desiredPosition,
    float radius) const
{
    glm::vec3 resolvedPosition = previousPosition;
    glm::vec3 candidate = previousPosition;

    const int axisOrder[3] = { 0, 2, 1 };
    for (int axisIndex = 0; axisIndex < 3; ++axisIndex)
    {
        const int axis = axisOrder[axisIndex];
        candidate = resolvedPosition;
        candidate[axis] = desiredPosition[axis];

        if (!IsCameraColliding(colliders, candidate, radius))
        {
            resolvedPosition = candidate;
        }
    }

    return resolvedPosition;
}

bool CollisionSystem::Intersects(const BoxCollider& a, const BoxCollider& b) const
{
    const OrientedBox boxA = BuildOrientedBox(a);
    const OrientedBox boxB = BuildOrientedBox(b);

    float rotation[3][3] = {};
    float absoluteRotation[3][3] = {};

    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            rotation[i][j] = glm::dot(boxA.axis[i], boxB.axis[j]);
            absoluteRotation[i][j] = std::abs(rotation[i][j]) + kEpsilon;
        }
    }

    const glm::vec3 centerDelta = boxB.center - boxA.center;
    const glm::vec3 translation(
        glm::dot(centerDelta, boxA.axis[0]),
        glm::dot(centerDelta, boxA.axis[1]),
        glm::dot(centerDelta, boxA.axis[2]));

    for (int i = 0; i < 3; ++i)
    {
        const float ra = boxA.halfExtents[i];
        const float rb =
            boxB.halfExtents[0] * absoluteRotation[i][0] +
            boxB.halfExtents[1] * absoluteRotation[i][1] +
            boxB.halfExtents[2] * absoluteRotation[i][2];
        if (std::abs(translation[i]) > ra + rb)
        {
            return false;
        }
    }

    for (int j = 0; j < 3; ++j)
    {
        const float ra =
            boxA.halfExtents[0] * absoluteRotation[0][j] +
            boxA.halfExtents[1] * absoluteRotation[1][j] +
            boxA.halfExtents[2] * absoluteRotation[2][j];
        const float rb = boxB.halfExtents[j];
        const float projectedTranslation =
            std::abs(
                translation[0] * rotation[0][j] +
                translation[1] * rotation[1][j] +
                translation[2] * rotation[2][j]);
        if (projectedTranslation > ra + rb)
        {
            return false;
        }
    }

    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            const float ra =
                boxA.halfExtents[(i + 1) % 3] * absoluteRotation[(i + 2) % 3][j] +
                boxA.halfExtents[(i + 2) % 3] * absoluteRotation[(i + 1) % 3][j];
            const float rb =
                boxB.halfExtents[(j + 1) % 3] * absoluteRotation[i][(j + 2) % 3] +
                boxB.halfExtents[(j + 2) % 3] * absoluteRotation[i][(j + 1) % 3];
            const float projectedTranslation = std::abs(
                translation[(i + 2) % 3] * rotation[(i + 1) % 3][j] -
                translation[(i + 1) % 3] * rotation[(i + 2) % 3][j]);
            if (projectedTranslation > ra + rb)
            {
                return false;
            }
        }
    }

    return true;
}
