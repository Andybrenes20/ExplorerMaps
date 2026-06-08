#include "Optimization.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>

#include "PhysicsWorld.h"

namespace {
    glm::vec4 NormalizePlane(const glm::vec4& plane) {
        const float length = glm::length(glm::vec3(plane));
        if (length <= 0.0001f) {
            return plane;
        }
        return plane / length;
    }

    float SmoothStep(float edge0, float edge1, float value) {
        const float width = std::max(edge1 - edge0, 0.0001f);
        const float t = std::clamp((value - edge0) / width, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    bool IntersectsSphere(const glm::vec4 planes[6], const glm::vec3& center, float radius) {
        for (const glm::vec4& plane : std::array<glm::vec4, 6>{
            planes[0], planes[1], planes[2], planes[3], planes[4], planes[5] }) {
            if (glm::dot(glm::vec3(plane), center) + plane.w < -radius) {
                return false;
            }
        }
        return true;
    }

    bool ProjectSphereToViewport(
        const Optimization::FrameCulling& frame,
        const glm::vec3& worldCenter,
        float worldRadius,
        glm::vec2& outCenter,
        float& outRadius) {
        const glm::vec4 clipCenter = frame.viewProjection * glm::vec4(worldCenter, 1.0f);
        if (std::abs(clipCenter.w) <= 0.0001f) {
            return false;
        }

        outCenter = glm::vec2(clipCenter) / clipCenter.w;
        const glm::vec4 clipOffset = frame.viewProjection * glm::vec4(worldCenter + frame.cameraRight * worldRadius, 1.0f);
        if (std::abs(clipOffset.w) > 0.0001f) {
            outRadius = glm::length((glm::vec2(clipOffset) / clipOffset.w) - outCenter);
        }
        else {
            outRadius = 0.0f;
        }

        if (!std::isfinite(outRadius) || outRadius <= 0.0001f) {
            const float distance = glm::length(frame.cameraPosition - worldCenter);
            outRadius = worldRadius / std::max(distance, 0.001f);
        }

        return std::isfinite(outCenter.x) && std::isfinite(outCenter.y);
    }

    bool RayIntersectsAabb(
        const glm::vec3& origin,
        const glm::vec3& direction,
        const glm::vec3& boundsMin,
        const glm::vec3& boundsMax,
        float maxDistance) {
        float tMin = 0.0f;
        float tMax = maxDistance;

        for (int axis = 0; axis < 3; ++axis) {
            const float rayOrigin = origin[axis];
            const float rayDirection = direction[axis];
            const float minValue = boundsMin[axis];
            const float maxValue = boundsMax[axis];

            if (std::abs(rayDirection) < 0.000001f) {
                if (rayOrigin < minValue || rayOrigin > maxValue) {
                    return false;
                }
                continue;
            }

            const float inverseDirection = 1.0f / rayDirection;
            float nearHit = (minValue - rayOrigin) * inverseDirection;
            float farHit = (maxValue - rayOrigin) * inverseDirection;
            if (nearHit > farHit) {
                std::swap(nearHit, farHit);
            }

            tMin = std::max(tMin, nearHit);
            tMax = std::min(tMax, farHit);
            if (tMin > tMax) {
                return false;
            }
        }

        return true;
    }

    bool IntersectRayTriangle(
        const glm::vec3& origin,
        const glm::vec3& direction,
        const glm::vec3& a,
        const glm::vec3& b,
        const glm::vec3& c,
        float& t) {
        const glm::vec3 edge1 = b - a;
        const glm::vec3 edge2 = c - a;
        const glm::vec3 pvec = glm::cross(direction, edge2);
        const float det = glm::dot(edge1, pvec);
        constexpr float epsilon = 0.0000001f;
        if (det > -epsilon && det < epsilon) {
            return false;
        }

        const float invDet = 1.0f / det;
        const glm::vec3 tvec = origin - a;
        const float u = glm::dot(tvec, pvec) * invDet;
        if (u < 0.0f || u > 1.0f) {
            return false;
        }

        const glm::vec3 qvec = glm::cross(tvec, edge1);
        const float v = glm::dot(direction, qvec) * invDet;
        if (v < 0.0f || u + v > 1.0f) {
            return false;
        }

        t = glm::dot(edge2, qvec) * invDet;
        return t > epsilon;
    }

    bool RaycastSubMesh(
        const CollisionMesh& collider,
        const CollisionSubMesh& subMesh,
        const glm::vec3& origin,
        const glm::vec3& direction,
        float& closestDistance) {
        const glm::vec3 padding(0.05f);
        if (!RayIntersectsAabb(origin, direction, subMesh.boundsMin - padding, subMesh.boundsMax + padding, closestDistance)) {
            return false;
        }

        bool hit = false;
        if (!subMesh.bvhNodes.empty()) {
            std::vector<int> pendingNodes;
            pendingNodes.reserve(64);
            pendingNodes.push_back(0);

            while (!pendingNodes.empty()) {
                const int nodeIndex = pendingNodes.back();
                pendingNodes.pop_back();
                const CollisionSubMesh::BvhNode& node = subMesh.bvhNodes[static_cast<std::size_t>(nodeIndex)];
                if (!RayIntersectsAabb(origin, direction, node.boundsMin - padding, node.boundsMax + padding, closestDistance)) {
                    continue;
                }

                if (node.triangleCount > 0) {
                    const std::size_t endTriangle = node.firstTriangle + node.triangleCount;
                    for (std::size_t triangle = node.firstTriangle; triangle < endTriangle; ++triangle) {
                        const std::size_t indexOffset = subMesh.bvhTriangleOffsets[triangle];
                        const glm::vec3& a = collider.vertices[collider.indices[indexOffset]];
                        const glm::vec3& b = collider.vertices[collider.indices[indexOffset + 1]];
                        const glm::vec3& c = collider.vertices[collider.indices[indexOffset + 2]];
                        float distance = 0.0f;
                        if (IntersectRayTriangle(origin, direction, a, b, c, distance) && distance < closestDistance) {
                            closestDistance = distance;
                            hit = true;
                        }
                    }
                }
                else {
                    if (node.left >= 0) {
                        pendingNodes.push_back(node.left);
                    }
                    if (node.right >= 0) {
                        pendingNodes.push_back(node.right);
                    }
                }
            }
            return hit;
        }

        const std::size_t endIndex = std::min(subMesh.indexStart + subMesh.indexCount, collider.indices.size());
        for (std::size_t i = subMesh.indexStart; i + 2 < endIndex; i += 3) {
            const glm::vec3& a = collider.vertices[collider.indices[i]];
            const glm::vec3& b = collider.vertices[collider.indices[i + 1]];
            const glm::vec3& c = collider.vertices[collider.indices[i + 2]];

            float distance = 0.0f;
            if (IntersectRayTriangle(origin, direction, a, b, c, distance) && distance < closestDistance) {
                closestDistance = distance;
                hit = true;
            }
        }
        return hit;
    }
}

namespace Optimization {
    std::size_t BuildCollisionAcceleration(
        const CollisionMesh& collider,
        std::vector<CollisionSubMesh>& subMeshes) {
        constexpr std::size_t minimumTriangles = 2048;
        constexpr std::size_t leafTriangles = 24;
        std::size_t acceleratedMeshes = 0;

        for (CollisionSubMesh& subMesh : subMeshes) {
            const std::size_t triangleCount = subMesh.indexCount / 3;
            if (triangleCount < minimumTriangles) {
                continue;
            }

            subMesh.bvhTriangleOffsets.resize(triangleCount);
            for (std::size_t triangle = 0; triangle < triangleCount; ++triangle) {
                subMesh.bvhTriangleOffsets[triangle] = subMesh.indexStart + triangle * 3;
            }
            subMesh.bvhNodes.reserve(triangleCount / leafTriangles * 2 + 1);

            const auto buildNode = [&](auto&& self, std::size_t first, std::size_t count) -> int {
                CollisionSubMesh::BvhNode node;
                node.boundsMin = glm::vec3(std::numeric_limits<float>::max());
                node.boundsMax = glm::vec3(-std::numeric_limits<float>::max());

                for (std::size_t i = first; i < first + count; ++i) {
                    const std::size_t offset = subMesh.bvhTriangleOffsets[i];
                    const glm::vec3& a = collider.vertices[collider.indices[offset]];
                    const glm::vec3& b = collider.vertices[collider.indices[offset + 1]];
                    const glm::vec3& c = collider.vertices[collider.indices[offset + 2]];
                    node.boundsMin = glm::min(node.boundsMin, glm::min(a, glm::min(b, c)));
                    node.boundsMax = glm::max(node.boundsMax, glm::max(a, glm::max(b, c)));
                }

                const int nodeIndex = static_cast<int>(subMesh.bvhNodes.size());
                subMesh.bvhNodes.push_back(node);
                if (count <= leafTriangles) {
                    subMesh.bvhNodes[static_cast<std::size_t>(nodeIndex)].firstTriangle = first;
                    subMesh.bvhNodes[static_cast<std::size_t>(nodeIndex)].triangleCount = count;
                    return nodeIndex;
                }

                const std::size_t middle = first + count / 2;
                const int left = self(self, first, middle - first);
                const int right = self(self, middle, first + count - middle);
                subMesh.bvhNodes[static_cast<std::size_t>(nodeIndex)].left = left;
                subMesh.bvhNodes[static_cast<std::size_t>(nodeIndex)].right = right;
                return nodeIndex;
            };

            buildNode(buildNode, 0, triangleCount);
            ++acceleratedMeshes;
        }
        return acceleratedMeshes;
    }

    std::vector<MeshBounds> BuildMeshBounds(const std::vector<RenderMesh>& meshes) {
        std::vector<MeshBounds> bounds;
        bounds.reserve(meshes.size());

        for (const RenderMesh& mesh : meshes) {
            MeshBounds current;
            if (mesh.tempVertices.empty()) {
                bounds.push_back(current);
                continue;
            }

            current.min = mesh.tempVertices[0];
            current.max = mesh.tempVertices[0];
            for (const glm::vec3& vertex : mesh.tempVertices) {
                current.min = glm::min(current.min, vertex);
                current.max = glm::max(current.max, vertex);
            }

            current.center = (current.min + current.max) * 0.5f;
            current.radius = glm::length(current.max - current.min) * 0.5f;
            current.valid = current.radius > 0.0001f;
            bounds.push_back(current);
        }

        return bounds;
    }

    SceneBounds BuildSceneBounds(const std::vector<MeshBounds>& meshBounds) {
        SceneBounds scene;
        bool first = true;
        for (const MeshBounds& bounds : meshBounds) {
            if (!bounds.valid) {
                continue;
            }

            if (first) {
                scene.min = bounds.min;
                scene.max = bounds.max;
                first = false;
            }
            else {
                scene.min = glm::min(scene.min, bounds.min);
                scene.max = glm::max(scene.max, bounds.max);
            }
        }

        if (!first) {
            scene.center = (scene.min + scene.max) * 0.5f;
            scene.radius = std::max(glm::length(scene.max - scene.min) * 0.5f, 1.0f);
            scene.valid = true;
        }
        return scene;
    }

    FrameCulling BuildFrameCulling(
        const glm::mat4& projection,
        const glm::mat4& view,
        const glm::vec3& cameraPosition,
        const glm::vec3& cameraForward,
        float sceneRadius) {
        FrameCulling frame;
        frame.viewProjection = projection * view;
        frame.cameraPosition = cameraPosition;
        frame.cameraForward = glm::length(cameraForward) > 0.0001f ? glm::normalize(cameraForward) : glm::vec3(0.0f, 0.0f, -1.0f);
        frame.cameraRight = glm::cross(frame.cameraForward, glm::vec3(0.0f, 1.0f, 0.0f));
        if (glm::length(frame.cameraRight) <= 0.0001f) {
            frame.cameraRight = glm::vec3(1.0f, 0.0f, 0.0f);
        }
        else {
            frame.cameraRight = glm::normalize(frame.cameraRight);
        }
        frame.sceneRadius = std::max(sceneRadius, 1.0f);

        const glm::mat4& m = frame.viewProjection;
        frame.planes[0] = NormalizePlane(glm::vec4(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0], m[3][3] + m[3][0]));
        frame.planes[1] = NormalizePlane(glm::vec4(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0], m[3][3] - m[3][0]));
        frame.planes[2] = NormalizePlane(glm::vec4(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1], m[3][3] + m[3][1]));
        frame.planes[3] = NormalizePlane(glm::vec4(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1], m[3][3] - m[3][1]));
        frame.planes[4] = NormalizePlane(glm::vec4(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2], m[3][3] + m[3][2]));
        frame.planes[5] = NormalizePlane(glm::vec4(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2], m[3][3] - m[3][2]));
        return frame;
    }

    bool ShouldDrawMesh(const MeshBounds& bounds, const FrameCulling& frame, const RenderSettings& settings) {
        if (!bounds.valid) {
            return true;
        }

        const glm::vec3 toMesh = bounds.center - frame.cameraPosition;
        const float distanceSquared = glm::dot(toMesh, toMesh);
        const float visibleDistance = settings.maxRenderDistance + bounds.radius;
        if (distanceSquared > visibleDistance * visibleDistance) {
            return false;
        }

        if (!IntersectsSphere(frame.planes, bounds.center, bounds.radius)) {
            return false;
        }

        glm::vec2 projectedCenter(0.0f);
        float screenRadius = 0.0f;
        if (!ProjectSphereToViewport(frame, bounds.center, bounds.radius, projectedCenter, screenRadius)) {
            return false;
        }

        const float paddedRadius = screenRadius + settings.viewportCullPadding;
        if (projectedCenter.x < -1.0f - paddedRadius ||
            projectedCenter.x > 1.0f + paddedRadius ||
            projectedCenter.y < -1.0f - paddedRadius ||
            projectedCenter.y > 1.0f + paddedRadius) {
            return false;
        }

        const float cameraDistance = std::sqrt(distanceSquared);
        if (cameraDistance <= frame.sceneRadius * settings.lodNearDistanceMultiplier) {
            return true;
        }

        const float viewportDistance = std::max(std::abs(projectedCenter.x), std::abs(projectedCenter.y));
        const float peripheralBoost = 1.0f + SmoothStep(settings.peripheralLodStart, settings.peripheralLodEnd, viewportDistance) * 0.85f;
        if (cameraDistance > frame.sceneRadius * settings.lodMidDistanceMultiplier) {
            return screenRadius >= settings.lodSmallMeshScreenRatio * peripheralBoost;
        }

        return screenRadius >= settings.lodTinyMeshScreenRatio * peripheralBoost;
    }

    RenderStats DrawCityMeshes(
        const std::vector<RenderMesh>& meshes,
        const std::vector<MeshBounds>& meshBounds,
        const FrameCulling& frame,
        const RenderSettings& settings) {
        RenderStats stats;
        for (std::size_t i = 0; i < meshes.size(); ++i) {
            if (i < meshBounds.size() && !ShouldDrawMesh(meshBounds[i], frame, settings)) {
                ++stats.culledMeshes;
                continue;
            }

            const RenderMesh& mesh = meshes[i];
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mesh.textureID);
            glBindVertexArray(mesh.VAO);
            glDrawElements(GL_TRIANGLES, mesh.numIndices, GL_UNSIGNED_INT, 0);
            ++stats.submittedMeshes;
        }
        return stats;
    }

    bool RaycastCollisionMeshes(
        const CollisionMesh& collider,
        const std::vector<CollisionSubMesh>& subMeshes,
        const glm::vec3& origin,
        const glm::vec3& direction,
        float& outDistance,
        int& lastHitMeshIndex) {
        if (collider.indices.empty() || collider.vertices.empty()) {
            return false;
        }

        if (subMeshes.empty()) {
            float closestDistance = std::numeric_limits<float>::max();
            bool hit = false;
            for (std::size_t i = 0; i + 2 < collider.indices.size(); i += 3) {
                float distance = 0.0f;
                if (IntersectRayTriangle(
                    origin,
                    direction,
                    collider.vertices[collider.indices[i]],
                    collider.vertices[collider.indices[i + 1]],
                    collider.vertices[collider.indices[i + 2]],
                    distance) &&
                    distance < closestDistance) {
                    closestDistance = distance;
                    hit = true;
                }
            }
            if (hit) {
                outDistance = closestDistance;
            }
            return hit;
        }

        float closestDistance = std::numeric_limits<float>::max();
        bool hit = false;
        int hitMeshIndex = -1;

        if (lastHitMeshIndex >= 0 && lastHitMeshIndex < static_cast<int>(subMeshes.size())) {
            if (RaycastSubMesh(collider, subMeshes[static_cast<std::size_t>(lastHitMeshIndex)], origin, direction, closestDistance)) {
                hit = true;
                hitMeshIndex = lastHitMeshIndex;
            }
        }

        for (std::size_t meshIndex = 0; meshIndex < subMeshes.size(); ++meshIndex) {
            if (static_cast<int>(meshIndex) == lastHitMeshIndex) {
                continue;
            }

            if (RaycastSubMesh(collider, subMeshes[meshIndex], origin, direction, closestDistance)) {
                hit = true;
                hitMeshIndex = static_cast<int>(meshIndex);
            }
        }

        if (hit) {
            outDistance = closestDistance;
            lastHitMeshIndex = hitMeshIndex;
        }
        return hit;
    }
}
