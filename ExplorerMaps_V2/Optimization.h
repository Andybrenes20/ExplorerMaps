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
        float maxRenderDistance = 100000.0f;
        float lodNearDistanceMultiplier = 1.0f;
        float lodMidDistanceMultiplier = 1.0f;
        float lodTinyMeshScreenRatio = 0.0f;
        float lodSmallMeshScreenRatio = 0.0f;
        float viewportCullPadding = 0.035f;
        float peripheralLodStart = 0.54f;
        float peripheralLodEnd = 1.0f;
        bool hideCristoArea = false;
        bool hideCristoCutoffArea = false;
        bool hideVolcanoArea = false;
        glm::vec3 cristoAreaMin = glm::vec3(-1500.0f, -80.0f, 180.0f);
        glm::vec3 cristoAreaMax = glm::vec3(140.0f, 900.0f, 1700.0f);
        glm::vec3 cristoForestAreaMin = glm::vec3(-1600.0f, -80.0f, 160.0f);
        glm::vec3 cristoForestAreaMax = glm::vec3(180.0f, 380.0f, 1740.0f);
        glm::vec3 cristoCutoffAreaMin = glm::vec3(-1600.0f, -80.0f, -160.0f);
        glm::vec3 cristoCutoffAreaMax = glm::vec3(180.0f, 900.0f, 1740.0f);
        glm::vec3 volcanoAreaMin = glm::vec3(-2450.0f, 80.0f, -1100.0f);
        glm::vec3 volcanoAreaMax = glm::vec3(-830.0f, 720.0f, 700.0f);
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
