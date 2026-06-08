#pragma once

#include <cstddef>
#include <vector>
#include <glm/glm.hpp>

struct CollisionMesh;
struct CollisionSubMesh;
struct RenderMesh;

namespace Optimization {
    struct MeshBounds {
        glm::vec3 min = glm::vec3(0.0f);
        glm::vec3 max = glm::vec3(0.0f);
        glm::vec3 center = glm::vec3(0.0f);
        float radius = 0.0f;
        bool valid = false;
    };

    struct SceneBounds {
        glm::vec3 min = glm::vec3(0.0f);
        glm::vec3 max = glm::vec3(0.0f);
        glm::vec3 center = glm::vec3(0.0f);
        float radius = 1.0f;
        bool valid = false;
    };

    struct RenderSettings {
        float maxRenderDistance = 900.0f;
        float lodNearDistanceMultiplier = 0.22f;
        float lodMidDistanceMultiplier = 0.46f;
        float lodTinyMeshScreenRatio = 0.0085f;
        float lodSmallMeshScreenRatio = 0.0200f;
        float viewportCullPadding = 0.035f;
        float peripheralLodStart = 0.54f;
        float peripheralLodEnd = 1.0f;
    };

    struct FrameCulling {
        glm::mat4 viewProjection = glm::mat4(1.0f);
        glm::vec4 planes[6]{};
        glm::vec3 cameraPosition = glm::vec3(0.0f);
        glm::vec3 cameraForward = glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 cameraRight = glm::vec3(1.0f, 0.0f, 0.0f);
        float sceneRadius = 1.0f;
    };

    struct RenderStats {
        std::size_t submittedMeshes = 0;
        std::size_t culledMeshes = 0;
    };

    std::vector<MeshBounds> BuildMeshBounds(const std::vector<RenderMesh>& meshes);
    SceneBounds BuildSceneBounds(const std::vector<MeshBounds>& meshBounds);
    FrameCulling BuildFrameCulling(
        const glm::mat4& projection,
        const glm::mat4& view,
        const glm::vec3& cameraPosition,
        const glm::vec3& cameraForward,
        float sceneRadius);

    bool ShouldDrawMesh(
        const MeshBounds& bounds,
        const FrameCulling& frame,
        const RenderSettings& settings = RenderSettings{});

    RenderStats DrawCityMeshes(
        const std::vector<RenderMesh>& meshes,
        const std::vector<MeshBounds>& meshBounds,
        const FrameCulling& frame,
        const RenderSettings& settings = RenderSettings{});

    std::size_t BuildCollisionAcceleration(
        const CollisionMesh& collider,
        std::vector<CollisionSubMesh>& subMeshes);

    bool RaycastCollisionMeshes(
        const CollisionMesh& collider,
        const std::vector<CollisionSubMesh>& subMeshes,
        const glm::vec3& origin,
        const glm::vec3& direction,
        float& outDistance,
        int& lastHitMeshIndex);
}
