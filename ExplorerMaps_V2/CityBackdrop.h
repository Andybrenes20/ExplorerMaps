#pragma once

#include <glm/glm.hpp>

class Shader;
class PhysicsWorld;

class CityBackdrop {
public:
    bool Initialize();
    void Shutdown();
    void AddCollisionTo(PhysicsWorld& physics) const;
    void Draw(Shader& shader, float landmarkFogIntensity, float cityFogIntensity, const glm::vec3& viewPosition, float currentFrame);

private:
    struct Vertex {
        glm::vec3 position;
        glm::vec2 uv;
        glm::vec3 normal;
    };

    void DrawBox(Shader& shader, const glm::vec3& center, const glm::vec3& size, const glm::vec3& tint, float alpha);
    void DrawGrayWall(Shader& shader);
    void DrawCristoBoundaryWalls(Shader& shader);
    void DrawCristoStreetRamp(Shader& shader);
    void DrawWaterSurface(Shader& shader, const glm::vec3& center, const glm::vec3& size, const glm::vec3& tint, float alpha);
    void DrawVoidCoverPlanes(Shader& shader);
    void DrawEdgeWaterLedges(Shader& shader, float currentFrame);
    void DrawWaterfallCurtains(Shader& shader, float currentFrame);
    void DrawOceanLayerWithCityGap(Shader& shader, const glm::vec3& center, const glm::vec3& size, const glm::vec3& tint, float alpha);

    unsigned int cubeVAO = 0;
    unsigned int cubeVBO = 0;
    unsigned int cubeEBO = 0;
    unsigned int whiteTexture = 0;
};
