#include "CityBackdrop.h"

#include "PhysicsWorld.h"
#include "Shader.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <vector>
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>

namespace {
    constexpr float kCityMinX = -650.0f;
    constexpr float kCityMaxX = 650.0f;
    constexpr float kCityMinZ = -780.0f;
    constexpr float kCityMaxZ = 650.0f;
    constexpr float kFlatOceanY = 25.62f;
    const glm::vec3 kGrayWallBase = glm::vec3(-377.9f, 25.0f, -306.9f);
    const glm::vec3 kGrayWallSize = glm::vec3(60.0f, 24.0f, 6.0f);
    const glm::vec3 kGrayWallTint = glm::vec3(0.62f, 0.63f, 0.64f);
    constexpr float kGrayWallYawDegrees = 40.0f;
    const glm::vec3 kSecondGrayWallStart = glm::vec3(-306.5f, 12.0f, -354.8f);
    const glm::vec3 kSecondGrayWallEnd = glm::vec3(-275.4f, 18.0f, -376.8f);
    constexpr float kSecondGrayWallHeight = 24.0f;
    constexpr float kSecondGrayWallThickness = 6.0f;
    constexpr float kCristoWallBaseY = 38.0f;
    constexpr float kCristoWallHeight = 72.0f;
    constexpr float kCristoWallThickness = 28.0f;
    const glm::vec3 kCristoWallTint = glm::vec3(0.56f, 0.58f, 0.55f);
    const glm::vec3 kCristoWallCapTint = glm::vec3(1.00f, 0.72f, 0.16f);
    const glm::vec3 kCristoWallPostTint = glm::vec3(0.11f, 0.13f, 0.13f);
    const glm::vec3 kRampConcreteTint = glm::vec3(0.45f, 0.47f, 0.46f);
    const glm::vec3 kRampEdgeTint = glm::vec3(0.95f, 0.66f, 0.12f);

    struct WallPlacement {
        glm::vec3 center;
        glm::vec3 size;
        float yawDegrees;
    };

    struct RampPlacement {
        glm::vec3 start;
        glm::vec3 end;
        float width;
        float thickness;
    };

    glm::vec3 GrayWallCenter() {
        return kGrayWallBase + glm::vec3(0.0f, kGrayWallSize.y * 0.5f, 0.0f);
    }

    glm::vec3 RotateY(const glm::vec3& offset, float yawDegrees) {
        const float yaw = glm::radians(yawDegrees);
        const float c = std::cos(yaw);
        const float s = std::sin(yaw);
        return glm::vec3(
            offset.x * c + offset.z * s,
            offset.y,
            -offset.x * s + offset.z * c);
    }

    glm::vec3 RotateYawPitch(const glm::vec3& offset, float yawDegrees, float pitchDegrees) {
        const float pitch = glm::radians(pitchDegrees);
        const float c = std::cos(pitch);
        const float s = std::sin(pitch);
        const glm::vec3 pitched(
            offset.x * c - offset.y * s,
            offset.x * s + offset.y * c,
            offset.z);
        return RotateY(pitched, yawDegrees);
    }

    WallPlacement MakeWallFromEndpoints(const glm::vec3& start, const glm::vec3& end, float height, float thickness) {
        const glm::vec3 delta(end.x - start.x, 0.0f, end.z - start.z);
        const float length = (std::max)(glm::length(glm::vec2(delta.x, delta.z)), 0.1f);
        const glm::vec3 baseCenter = (start + end) * 0.5f;
        return {
            baseCenter + glm::vec3(0.0f, height * 0.5f, 0.0f),
            glm::vec3(length, height, thickness),
            glm::degrees(std::atan2(-delta.z, delta.x))
        };
    }

    WallPlacement GrayWallPlacement() {
        return { GrayWallCenter(), kGrayWallSize, kGrayWallYawDegrees };
    }

    WallPlacement SecondGrayWallPlacement() {
        return MakeWallFromEndpoints(kSecondGrayWallStart, kSecondGrayWallEnd, kSecondGrayWallHeight, kSecondGrayWallThickness);
    }

    WallPlacement CristoBoundaryWall(int index) {
        const glm::vec3 starts[] = {
            glm::vec3(329.3f, kCristoWallBaseY, 420.0f),
            glm::vec3(151.0f, kCristoWallBaseY, 1730.7f),
            glm::vec3(-1169.6f, kCristoWallBaseY, 1556.5f),
            glm::vec3(329.3f, kCristoWallBaseY, 420.0f),
            glm::vec3(-994.6f, kCristoWallBaseY, 238.6f)
        };
        const glm::vec3 ends[] = {
            glm::vec3(151.0f, kCristoWallBaseY, 1730.7f),
            glm::vec3(-1169.6f, kCristoWallBaseY, 1556.5f),
            glm::vec3(-994.6f, kCristoWallBaseY, 238.6f),
            glm::vec3(-358.0f, kCristoWallBaseY, 326.2f),
            glm::vec3(-407.0f, kCristoWallBaseY, 321.7f)
        };
        return MakeWallFromEndpoints(starts[index], ends[index], kCristoWallHeight, kCristoWallThickness);
    }

    RampPlacement CristoStreetRamp() {
        return {
            glm::vec3(-383.0f, 51.0f, 338.0f),
            glm::vec3(-383.0f, 48.5f, 362.0f),
            38.0f,
            1.8f
        };
    }

    bool IsOutsideCity(const glm::vec3& position) {
        return position.x < kCityMinX ||
            position.x > kCityMaxX ||
            position.z < kCityMinZ ||
            position.z > kCityMaxZ;
    }

    void AddWallCollision(PhysicsWorld& physics, const WallPlacement& wall) {
        const glm::vec3 halfSize = wall.size * 0.5f;
        const auto point = [&](float x, float y, float z) {
            return wall.center + RotateY(glm::vec3(x, y, z), wall.yawDegrees);
        };

        physics.AddCollisionTriangles({
            point(-halfSize.x, -halfSize.y, -halfSize.z),
            point(halfSize.x, -halfSize.y, -halfSize.z),
            point(halfSize.x, halfSize.y, -halfSize.z),
            point(-halfSize.x, halfSize.y, -halfSize.z),
            point(-halfSize.x, -halfSize.y, halfSize.z),
            point(halfSize.x, -halfSize.y, halfSize.z),
            point(halfSize.x, halfSize.y, halfSize.z),
            point(-halfSize.x, halfSize.y, halfSize.z)
        }, {
            4, 5, 6, 6, 7, 4,
            1, 0, 3, 3, 2, 1,
            0, 4, 7, 7, 3, 0,
            5, 1, 2, 2, 6, 5,
            3, 7, 6, 6, 2, 3,
            0, 1, 5, 5, 4, 0
        });
    }

    void AddRampCollision(PhysicsWorld& physics, const RampPlacement& ramp) {
        const glm::vec3 delta = ramp.end - ramp.start;
        const float horizontalLength = (std::max)(glm::length(glm::vec2(delta.x, delta.z)), 0.1f);
        const float yaw = glm::degrees(std::atan2(-delta.z, delta.x));
        const float pitch = glm::degrees(std::atan2(delta.y, horizontalLength));
        const float rampLength = glm::length(delta);
        const float halfLength = rampLength * 0.5f;
        const float halfWidth = ramp.width * 0.5f;
        const float halfThickness = ramp.thickness * 0.5f;
        const glm::vec3 center = (ramp.start + ramp.end) * 0.5f - glm::vec3(0.0f, halfThickness, 0.0f);

        const auto point = [&](float x, float y, float z) {
            return center + RotateYawPitch(glm::vec3(x, y, z), yaw, pitch);
        };

        physics.AddCollisionTriangles({
            point(-halfLength, halfThickness, -halfWidth),
            point(halfLength, halfThickness, -halfWidth),
            point(halfLength, halfThickness, halfWidth),
            point(-halfLength, halfThickness, halfWidth),
            point(-halfLength, -halfThickness, -halfWidth),
            point(halfLength, -halfThickness, -halfWidth),
            point(halfLength, -halfThickness, halfWidth),
            point(-halfLength, -halfThickness, halfWidth)
        }, {
            0, 1, 2, 2, 3, 0,
            4, 7, 6, 6, 5, 4,
            0, 4, 5, 5, 1, 0,
            3, 2, 6, 6, 7, 3,
            0, 3, 7, 7, 4, 0,
            1, 5, 6, 6, 2, 1
        });
    }
}

bool CityBackdrop::Initialize() {
    const std::array<Vertex, 24> vertices = {
        Vertex{{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        Vertex{{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        Vertex{{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        Vertex{{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},

        Vertex{{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        Vertex{{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        Vertex{{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        Vertex{{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},

        Vertex{{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
        Vertex{{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
        Vertex{{-0.5f,  0.5f,  0.5f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
        Vertex{{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},

        Vertex{{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        Vertex{{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        Vertex{{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        Vertex{{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},

        Vertex{{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        Vertex{{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        Vertex{{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        Vertex{{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},

        Vertex{{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
        Vertex{{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
        Vertex{{ 0.5f, -0.5f,  0.5f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
        Vertex{{-0.5f, -0.5f,  0.5f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
    };

    const std::array<unsigned int, 36> indices = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        8, 9, 10, 10, 11, 8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20
    };

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);
    glBindVertexArray(cubeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);

    const unsigned char whitePixel[] = { 255, 255, 255, 255 };
    glGenTextures(1, &whiteTexture);
    glBindTexture(GL_TEXTURE_2D, whiteTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return cubeVAO != 0 && whiteTexture != 0;
}

void CityBackdrop::Shutdown() {
    if (cubeVAO != 0) glDeleteVertexArrays(1, &cubeVAO);
    if (cubeVBO != 0) glDeleteBuffers(1, &cubeVBO);
    if (cubeEBO != 0) glDeleteBuffers(1, &cubeEBO);
    if (whiteTexture != 0) glDeleteTextures(1, &whiteTexture);
    cubeVAO = cubeVBO = cubeEBO = whiteTexture = 0;
}

void CityBackdrop::AddCollisionTo(PhysicsWorld& physics) const {
    AddWallCollision(physics, GrayWallPlacement());
    AddWallCollision(physics, SecondGrayWallPlacement());
    for (int i = 0; i < 5; ++i) {
        const WallPlacement wall = CristoBoundaryWall(i);
        AddWallCollision(physics, wall);
    }
    AddRampCollision(physics, CristoStreetRamp());
}

void CityBackdrop::Draw(Shader& shader, float landmarkFogIntensity, float cityFogIntensity, const glm::vec3& viewPosition, float currentFrame) {
    if (cubeVAO == 0) {
        return;
    }

    shader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, whiteTexture);

    (void)landmarkFogIntensity;
    (void)cityFogIntensity;
    const bool outsideCity = IsOutsideCity(viewPosition);
    const float waveDrift = std::sin(currentFrame * 0.35f) * 34.0f;

    if (outsideCity) {
        DrawWaterSurface(shader, glm::vec3(-80.0f, kFlatOceanY, 780.0f), glm::vec3(9800.0f, 0.18f, 8800.0f), glm::vec3(0.06f, 0.24f, 0.38f), 1.0f);
    }
    else {
        DrawOceanLayerWithCityGap(shader, glm::vec3(-80.0f, 25.2f, 780.0f), glm::vec3(9800.0f, 0.55f, 8800.0f), glm::vec3(0.025f, 0.13f, 0.24f), 1.0f);
        DrawWaterSurface(shader, glm::vec3(210.0f, -18.0f, 190.0f), glm::vec3(2200.0f, 0.20f, 1990.0f), glm::vec3(0.02f, 0.11f, 0.20f), 1.0f);
        DrawVoidCoverPlanes(shader);
    }
    DrawGrayWall(shader);
    DrawCristoBoundaryWalls(shader);
    DrawCristoStreetRamp(shader);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    if (outsideCity) {
        DrawWaterSurface(shader, glm::vec3(-80.0f + waveDrift * 0.18f, kFlatOceanY + 0.08f, 780.0f), glm::vec3(9400.0f, 0.10f, 8350.0f), glm::vec3(0.12f, 0.36f, 0.54f), 0.36f);
        DrawWaterSurface(shader, glm::vec3(-80.0f - waveDrift * 0.12f, kFlatOceanY + 0.16f, 780.0f), glm::vec3(8850.0f, 0.08f, 7750.0f), glm::vec3(0.30f, 0.60f, 0.74f), 0.18f);
    }
    else {
        DrawOceanLayerWithCityGap(shader, glm::vec3(-80.0f + waveDrift, 25.62f, 780.0f), glm::vec3(9400.0f, 0.12f, 8350.0f), glm::vec3(0.09f, 0.33f, 0.52f), 0.46f);
        DrawOceanLayerWithCityGap(shader, glm::vec3(-80.0f - waveDrift * 0.55f, 25.78f, 780.0f), glm::vec3(8850.0f, 0.10f, 7750.0f), glm::vec3(0.28f, 0.58f, 0.72f), 0.24f);
        DrawEdgeWaterLedges(shader, currentFrame);
        DrawWaterfallCurtains(shader, currentFrame);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    shader.setFloat("objectAlpha", 1.0f);
    shader.setVec3("objectTint", glm::vec3(1.0f));
    glBindVertexArray(0);
}

void CityBackdrop::DrawBox(Shader& shader, const glm::vec3& center, const glm::vec3& size, const glm::vec3& tint, float alpha) {
    const glm::mat4 model = glm::translate(glm::mat4(1.0f), center) * glm::scale(glm::mat4(1.0f), size);
    shader.setMat4("model", model);
    shader.setVec3("objectTint", tint);
    shader.setFloat("objectAlpha", alpha);
    glBindVertexArray(cubeVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
}

void CityBackdrop::DrawGrayWall(Shader& shader) {
    const WallPlacement walls[] = {
        GrayWallPlacement(),
        SecondGrayWallPlacement()
    };

    for (const WallPlacement& wall : walls) {
        const glm::mat4 model =
            glm::translate(glm::mat4(1.0f), wall.center) *
            glm::rotate(glm::mat4(1.0f), glm::radians(wall.yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::scale(glm::mat4(1.0f), wall.size);
        shader.setMat4("model", model);
        shader.setVec3("objectTint", kGrayWallTint);
        shader.setFloat("objectAlpha", 1.0f);
        glBindVertexArray(cubeVAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
    }
}

void CityBackdrop::DrawCristoBoundaryWalls(Shader& shader) {
    for (int wallIndex = 0; wallIndex < 5; ++wallIndex) {
        const WallPlacement wall = CristoBoundaryWall(wallIndex);
        const glm::mat4 bodyModel =
            glm::translate(glm::mat4(1.0f), wall.center) *
            glm::rotate(glm::mat4(1.0f), glm::radians(wall.yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::scale(glm::mat4(1.0f), wall.size);
        shader.setMat4("model", bodyModel);
        shader.setVec3("objectTint", kCristoWallTint);
        shader.setFloat("objectAlpha", 1.0f);
        glBindVertexArray(cubeVAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);

        const glm::mat4 capModel =
            glm::translate(glm::mat4(1.0f), wall.center + glm::vec3(0.0f, wall.size.y * 0.5f + 1.6f, 0.0f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(wall.yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::scale(glm::mat4(1.0f), glm::vec3(wall.size.x + 6.0f, 3.2f, wall.size.z + 7.0f));
        shader.setMat4("model", capModel);
        shader.setVec3("objectTint", kCristoWallCapTint);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);

        const glm::mat4 stripeModel =
            glm::translate(glm::mat4(1.0f), wall.center + glm::vec3(0.0f, 5.0f, 0.0f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(wall.yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::scale(glm::mat4(1.0f), glm::vec3(wall.size.x + 3.0f, 3.5f, wall.size.z + 8.0f));
        shader.setMat4("model", stripeModel);
        shader.setVec3("objectTint", glm::vec3(0.12f, 0.14f, 0.14f));
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);

        const int postCount = (std::max)(2, static_cast<int>(wall.size.x / 160.0f) + 1);
        for (int i = 0; i <= postCount; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(postCount);
            const float localX = -wall.size.x * 0.5f + wall.size.x * t;
            const glm::vec3 postCenter = wall.center + RotateY(glm::vec3(localX, 8.0f, 0.0f), wall.yawDegrees);
            const glm::mat4 postModel =
                glm::translate(glm::mat4(1.0f), postCenter) *
                glm::rotate(glm::mat4(1.0f), glm::radians(wall.yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f)) *
                glm::scale(glm::mat4(1.0f), glm::vec3(10.0f, wall.size.y + 16.0f, wall.size.z + 10.0f));
            shader.setMat4("model", postModel);
            shader.setVec3("objectTint", kCristoWallPostTint);
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
        }
    }
}

void CityBackdrop::DrawCristoStreetRamp(Shader& shader) {
    const RampPlacement ramp = CristoStreetRamp();
    const glm::vec3 delta = ramp.end - ramp.start;
    const float horizontalLength = (std::max)(glm::length(glm::vec2(delta.x, delta.z)), 0.1f);
    const float yaw = glm::degrees(std::atan2(-delta.z, delta.x));
    const float pitch = glm::degrees(std::atan2(delta.y, horizontalLength));
    const float rampLength = glm::length(delta);
    const glm::vec3 center = (ramp.start + ramp.end) * 0.5f - glm::vec3(0.0f, ramp.thickness * 0.5f, 0.0f);

    const glm::mat4 rampModel =
        glm::translate(glm::mat4(1.0f), center) *
        glm::rotate(glm::mat4(1.0f), glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f)) *
        glm::rotate(glm::mat4(1.0f), glm::radians(pitch), glm::vec3(0.0f, 0.0f, 1.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(rampLength, ramp.thickness, ramp.width));
    shader.setMat4("model", rampModel);
    shader.setVec3("objectTint", kRampConcreteTint);
    shader.setFloat("objectAlpha", 1.0f);
    glBindVertexArray(cubeVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);

    const float edgeOffset = ramp.width * 0.5f - 2.2f;
    const float sides[] = { -1.0f, 1.0f };
    for (float side : sides) {
        const glm::vec3 edgeCenter = center + RotateYawPitch(glm::vec3(0.0f, ramp.thickness * 0.9f, side * edgeOffset), yaw, pitch);
        const glm::mat4 edgeModel =
            glm::translate(glm::mat4(1.0f), edgeCenter) *
            glm::rotate(glm::mat4(1.0f), glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::rotate(glm::mat4(1.0f), glm::radians(pitch), glm::vec3(0.0f, 0.0f, 1.0f)) *
            glm::scale(glm::mat4(1.0f), glm::vec3(rampLength + 2.0f, 1.0f, 2.2f));
        shader.setMat4("model", edgeModel);
        shader.setVec3("objectTint", kRampEdgeTint);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
    }
}

void CityBackdrop::DrawWaterSurface(Shader& shader, const glm::vec3& center, const glm::vec3& size, const glm::vec3& tint, float alpha) {
    const glm::mat4 model = glm::translate(glm::mat4(1.0f), center) * glm::scale(glm::mat4(1.0f), size);
    shader.setMat4("model", model);
    shader.setVec3("objectTint", tint);
    shader.setFloat("objectAlpha", alpha);
    glBindVertexArray(cubeVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, reinterpret_cast<void*>(24 * sizeof(unsigned int)));
}

void CityBackdrop::DrawVoidCoverPlanes(Shader& shader) {
    const float cityMinX = kCityMinX;
    const float cityMaxX = kCityMaxX;
    const float cityMinZ = kCityMinZ;
    const float cityMaxZ = kCityMaxZ;
    const float topY = 25.8f;
    const float bottomY = -22.0f;
    const float height = topY - bottomY;
    const float centerY = (topY + bottomY) * 0.5f;
    const float thickness = 14.0f;
    const glm::vec3 oceanWallTint(0.018f, 0.105f, 0.19f);

    DrawBox(shader,
        glm::vec3(cityMinX + thickness * 0.5f, centerY, (cityMinZ + cityMaxZ) * 0.5f),
        glm::vec3(thickness, height, cityMaxZ - cityMinZ + thickness * 2.0f),
        oceanWallTint,
        1.0f);
    DrawBox(shader,
        glm::vec3(cityMaxX - thickness * 0.5f, centerY, (cityMinZ + cityMaxZ) * 0.5f),
        glm::vec3(thickness, height, cityMaxZ - cityMinZ + thickness * 2.0f),
        oceanWallTint,
        1.0f);
    DrawBox(shader,
        glm::vec3((cityMinX + cityMaxX) * 0.5f, centerY, cityMinZ + thickness * 0.5f),
        glm::vec3(cityMaxX - cityMinX + thickness * 2.0f, height, thickness),
        oceanWallTint,
        1.0f);
    DrawBox(shader,
        glm::vec3((cityMinX + cityMaxX) * 0.5f, centerY, cityMaxZ - thickness * 0.5f),
        glm::vec3(cityMaxX - cityMinX + thickness * 2.0f, height, thickness),
        oceanWallTint,
        1.0f);
}

void CityBackdrop::DrawEdgeWaterLedges(Shader& shader, float currentFrame) {
    const float cityMinX = kCityMinX;
    const float cityMaxX = kCityMaxX;
    const float cityMinZ = kCityMinZ;
    const float cityMaxZ = kCityMaxZ;
    const float ledgeWidth = 72.0f;
    const float ledgeY = 25.48f + std::sin(currentFrame * 0.55f) * 0.08f;
    const glm::vec3 ledgeTint(0.025f, 0.13f, 0.24f);

    DrawWaterSurface(shader,
        glm::vec3(cityMinX + ledgeWidth * 0.5f, ledgeY, (cityMinZ + cityMaxZ) * 0.5f),
        glm::vec3(ledgeWidth, 0.10f, cityMaxZ - cityMinZ),
        ledgeTint,
        0.52f);
    DrawWaterSurface(shader,
        glm::vec3(cityMaxX - ledgeWidth * 0.5f, ledgeY, (cityMinZ + cityMaxZ) * 0.5f),
        glm::vec3(ledgeWidth, 0.10f, cityMaxZ - cityMinZ),
        ledgeTint,
        0.52f);
    DrawWaterSurface(shader,
        glm::vec3((cityMinX + cityMaxX) * 0.5f, ledgeY, cityMinZ + ledgeWidth * 0.5f),
        glm::vec3(cityMaxX - cityMinX, 0.10f, ledgeWidth),
        ledgeTint,
        0.52f);
    DrawWaterSurface(shader,
        glm::vec3((cityMinX + cityMaxX) * 0.5f, ledgeY, cityMaxZ - ledgeWidth * 0.5f),
        glm::vec3(cityMaxX - cityMinX, 0.10f, ledgeWidth),
        ledgeTint,
        0.52f);
}

void CityBackdrop::DrawWaterfallCurtains(Shader& shader, float currentFrame) {
    const float cityMinX = kCityMinX;
    const float cityMaxX = kCityMaxX;
    const float cityMinZ = kCityMinZ;
    const float cityMaxZ = kCityMaxZ;
    const float curtainY = 3.6f;
    const float curtainHeight = 43.0f;
    const glm::vec3 curtainTint(0.025f, 0.13f, 0.24f);
    const glm::vec3 deepCurtainTint(0.02f, 0.11f, 0.20f);

    DrawBox(shader, glm::vec3(cityMinX + 9.0f, curtainY - 1.0f, (cityMinZ + cityMaxZ) * 0.5f), glm::vec3(22.0f, curtainHeight + 4.0f, cityMaxZ - cityMinZ), deepCurtainTint, 0.92f);
    DrawBox(shader, glm::vec3(cityMaxX - 9.0f, curtainY - 1.0f, (cityMinZ + cityMaxZ) * 0.5f), glm::vec3(22.0f, curtainHeight + 4.0f, cityMaxZ - cityMinZ), deepCurtainTint, 0.92f);
    DrawBox(shader, glm::vec3((cityMinX + cityMaxX) * 0.5f, curtainY - 1.0f, cityMinZ + 9.0f), glm::vec3(cityMaxX - cityMinX, curtainHeight + 4.0f, 22.0f), deepCurtainTint, 0.92f);
    DrawBox(shader, glm::vec3((cityMinX + cityMaxX) * 0.5f, curtainY - 1.0f, cityMaxZ - 9.0f), glm::vec3(cityMaxX - cityMinX, curtainHeight + 4.0f, 22.0f), deepCurtainTint, 0.92f);

    DrawBox(shader, glm::vec3(cityMinX, curtainY, (cityMinZ + cityMaxZ) * 0.5f), glm::vec3(22.0f, curtainHeight, cityMaxZ - cityMinZ), curtainTint, 0.86f);
    DrawBox(shader, glm::vec3(cityMaxX, curtainY, (cityMinZ + cityMaxZ) * 0.5f), glm::vec3(22.0f, curtainHeight, cityMaxZ - cityMinZ), curtainTint, 0.86f);
    DrawBox(shader, glm::vec3((cityMinX + cityMaxX) * 0.5f, curtainY, cityMinZ), glm::vec3(cityMaxX - cityMinX, curtainHeight, 22.0f), curtainTint, 0.86f);
    DrawBox(shader, glm::vec3((cityMinX + cityMaxX) * 0.5f, curtainY, cityMaxZ), glm::vec3(cityMaxX - cityMinX, curtainHeight, 22.0f), curtainTint, 0.86f);

    const glm::vec3 streakTint(0.09f, 0.33f, 0.52f);
    for (int i = 0; i < 18; ++i) {
        const float t = static_cast<float>(i) / 17.0f;
        const float z = cityMinZ + (cityMaxZ - cityMinZ) * t;
        const float pulse = 0.5f + 0.5f * std::sin(currentFrame * 1.7f + static_cast<float>(i) * 0.83f);
        DrawBox(shader, glm::vec3(cityMinX - 2.2f, curtainY - pulse * 2.0f, z), glm::vec3(6.5f, curtainHeight * (0.70f + pulse * 0.18f), 24.0f), streakTint, 0.42f);
        DrawBox(shader, glm::vec3(cityMaxX + 2.2f, curtainY - pulse * 2.0f, z), glm::vec3(6.5f, curtainHeight * (0.70f + pulse * 0.18f), 24.0f), streakTint, 0.42f);
    }
    for (int i = 0; i < 18; ++i) {
        const float t = static_cast<float>(i) / 17.0f;
        const float x = cityMinX + (cityMaxX - cityMinX) * t;
        const float pulse = 0.5f + 0.5f * std::sin(currentFrame * 1.5f + static_cast<float>(i) * 0.91f);
        DrawBox(shader, glm::vec3(x, curtainY - pulse * 2.0f, cityMinZ - 2.2f), glm::vec3(24.0f, curtainHeight * (0.70f + pulse * 0.18f), 6.5f), streakTint, 0.42f);
        DrawBox(shader, glm::vec3(x, curtainY - pulse * 2.0f, cityMaxZ + 2.2f), glm::vec3(24.0f, curtainHeight * (0.70f + pulse * 0.18f), 6.5f), streakTint, 0.42f);
    }
}

void CityBackdrop::DrawOceanLayerWithCityGap(Shader& shader, const glm::vec3& center, const glm::vec3& size, const glm::vec3& tint, float alpha) {
    const float minX = center.x - size.x * 0.5f;
    const float maxX = center.x + size.x * 0.5f;
    const float minZ = center.z - size.z * 0.5f;
    const float maxZ = center.z + size.z * 0.5f;

    const float cityMinX = kCityMinX;
    const float cityMaxX = kCityMaxX;
    const float cityMinZ = kCityMinZ;
    const float cityMaxZ = kCityMaxZ;

    if (cityMinX > minX) {
        DrawWaterSurface(shader,
            glm::vec3((minX + cityMinX) * 0.5f, center.y, center.z),
            glm::vec3(cityMinX - minX, size.y, size.z),
            tint,
            alpha);
    }
    if (cityMaxX < maxX) {
        DrawWaterSurface(shader,
            glm::vec3((cityMaxX + maxX) * 0.5f, center.y, center.z),
            glm::vec3(maxX - cityMaxX, size.y, size.z),
            tint,
            alpha);
    }
    if (cityMinZ > minZ) {
        DrawWaterSurface(shader,
            glm::vec3((cityMinX + cityMaxX) * 0.5f, center.y, (minZ + cityMinZ) * 0.5f),
            glm::vec3(cityMaxX - cityMinX, size.y, cityMinZ - minZ),
            tint,
            alpha);
    }
    if (cityMaxZ < maxZ) {
        DrawWaterSurface(shader,
            glm::vec3((cityMinX + cityMaxX) * 0.5f, center.y, (cityMaxZ + maxZ) * 0.5f),
            glm::vec3(cityMaxX - cityMinX, size.y, maxZ - cityMaxZ),
            tint,
            alpha);
    }
}
