#include "PickingSystem.h"

#include <cfloat>
#include <cmath>

#include <glm/gtc/matrix_inverse.hpp>

namespace
{
    struct Ray
    {
        glm::vec3 origin = glm::vec3(0.0f);
        glm::vec3 direction = glm::vec3(0.0f, 0.0f, -1.0f);
    };

    Ray BuildRay(const PickingViewport& viewport)
    {
        const float ndcX = (2.0f * viewport.mousePosition.x / viewport.viewportSize.x) - 1.0f;
        const float ndcY = 1.0f - (2.0f * viewport.mousePosition.y / viewport.viewportSize.y);

        const glm::mat4 inverseViewProjection = glm::inverse(viewport.projection * viewport.view);
        glm::vec4 farPoint = inverseViewProjection * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
        farPoint /= farPoint.w;

        Ray ray;
        ray.origin = viewport.cameraPosition;
        ray.direction = glm::normalize(glm::vec3(farPoint) - ray.origin);
        return ray;
    }

    float IntersectSphere(const Ray& ray, const glm::vec3& center, float radius)
    {
        const glm::vec3 offset = ray.origin - center;
        const float a = glm::dot(ray.direction, ray.direction);
        const float b = 2.0f * glm::dot(offset, ray.direction);
        const float c = glm::dot(offset, offset) - radius * radius;
        const float discriminant = (b * b) - (4.0f * a * c);
        if (discriminant < 0.0f)
        {
            return FLT_MAX;
        }

        const float sqrtDiscriminant = std::sqrt(discriminant);
        const float t0 = (-b - sqrtDiscriminant) / (2.0f * a);
        const float t1 = (-b + sqrtDiscriminant) / (2.0f * a);
        if (t0 > 0.0f)
        {
            return t0;
        }

        return t1 > 0.0f ? t1 : FLT_MAX;
    }
}

SceneSelection PickingSystem::Pick(const EditorSceneData& scene, const PickingViewport& viewport) const
{
    SceneSelection result;
    const Ray ray = BuildRay(viewport);
    float closestDistance = FLT_MAX;

    for (int i = 0; i < static_cast<int>(scene.entities.size()); ++i)
    {
        const Entity& entity = scene.entities[i];
        const float distance = IntersectSphere(ray, entity.position, std::max(entity.pickRadius * MaxComponent(entity.scale), 1.0f));
        if (distance < closestDistance)
        {
            closestDistance = distance;
            result.type = SceneObjectType::Entity;
            result.index = i;
        }
    }

    for (int i = 0; i < static_cast<int>(scene.lights.size()); ++i)
    {
        const Light& light = scene.lights[i];
        const float radius = std::max(light.helperSize, light.radius * 0.25f);
        const float distance = IntersectSphere(ray, light.position, std::max(radius, 1.0f));
        if (distance < closestDistance)
        {
            closestDistance = distance;
            result.type = SceneObjectType::Light;
            result.index = i;
        }
    }

    for (int i = 0; i < static_cast<int>(scene.helpers.size()); ++i)
    {
        const Helper& helper = scene.helpers[i];
        const float distance = IntersectSphere(ray, helper.position, std::max(MaxComponent(helper.scale), 1.0f));
        if (distance < closestDistance)
        {
            closestDistance = distance;
            result.type = SceneObjectType::Helper;
            result.index = i;
        }
    }

    return result;
}
