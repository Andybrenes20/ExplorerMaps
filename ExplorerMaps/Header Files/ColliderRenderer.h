#pragma once

#include <vector>

#include "Camara.h"
#include "ColliderTypes.h"
#include "CollisionSystem.h"
#include "shaderClass.h"

class ColliderRenderer
{
public:
    void Init(const char* vertexShaderPath, const char* fragmentShaderPath);
    void Render(
        const std::vector<BoxCollider>& colliders,
        const BoxCollider* previewCollider,
        const Camera& camera,
        float FOVdeg,
        float nearPlane,
        float farPlane,
        const CollisionSystem& collisionSystem,
        int selectedIndex,
        const glm::vec3& cameraPosition,
        float cameraRadius,
        bool drawSolid,
        bool drawWireframe,
        bool drawThroughWalls);
    void Shutdown();

private:
    void EnsureGeometry();

    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;
    Shader* shader = nullptr;
};

