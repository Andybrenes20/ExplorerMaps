#include "ColliderRenderer.h"

#include <glm/gtc/type_ptr.hpp>

namespace
{
    constexpr float kCubeVertices[] =
    {
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f
    };

    constexpr unsigned int kCubeIndices[] =
    {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        0, 4, 7, 7, 3, 0,
        1, 5, 6, 6, 2, 1,
        3, 2, 6, 6, 7, 3,
        0, 1, 5, 5, 4, 0
    };

    glm::vec4 ResolveColor(
        const std::vector<BoxCollider>& colliders,
        const CollisionSystem& collisionSystem,
        int index,
        int selectedIndex,
        const glm::vec3& cameraPosition,
        float cameraRadius)
    {
        const BoxCollider& collider = colliders[index];
        if (collisionSystem.HasColliderIntersection(colliders, index) ||
            collisionSystem.IntersectsCamera(collider, cameraPosition, cameraRadius))
        {
            return glm::vec4(1.0f, 0.25f, 0.22f, 1.0f);
        }

        if (index == selectedIndex)
        {
            return glm::vec4(0.22f, 1.0f, 0.42f, 1.0f);
        }

        return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

void ColliderRenderer::Init(const char* vertexShaderPath, const char* fragmentShaderPath)
{
    if (!shader)
    {
        shader = new Shader(vertexShaderPath, fragmentShaderPath);
    }

    EnsureGeometry();
}

void ColliderRenderer::Render(
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
    bool drawThroughWalls)
{
    if (!shader || (colliders.empty() && !previewCollider))
    {
        return;
    }

    EnsureGeometry();
    shader->Activate();

    const glm::mat4 view = camera.GetViewMatrix();
    const glm::mat4 projection = camera.GetProjectionMatrix(FOVdeg, nearPlane, farPlane);
    glUniformMatrix4fv(shader->GetUniformLocation("view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(shader->GetUniformLocation("projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glBindVertexArray(VAO);
    if (drawThroughWalls)
    {
        glDisable(GL_DEPTH_TEST);
    }

    if (drawSolid)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        for (int i = 0; i < static_cast<int>(colliders.size()); ++i)
        {
            if (!colliders[i].visible)
            {
                continue;
            }

            const glm::mat4 model = ComposeColliderMatrix(colliders[i]);
            glm::vec4 color = ResolveColor(colliders, collisionSystem, i, selectedIndex, cameraPosition, cameraRadius);
            color.a = (i == selectedIndex) ? 0.22f : 0.12f;

            glUniformMatrix4fv(shader->GetUniformLocation("model"), 1, GL_FALSE, glm::value_ptr(model));
            glUniform4fv(shader->GetUniformLocation("tintColor"), 1, glm::value_ptr(color));
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
        }

        if (previewCollider)
        {
            const glm::mat4 model = ComposeColliderMatrix(*previewCollider);
            const glm::vec4 color(1.0f, 0.86f, 0.22f, 0.14f);
            glUniformMatrix4fv(shader->GetUniformLocation("model"), 1, GL_FALSE, glm::value_ptr(model));
            glUniform4fv(shader->GetUniformLocation("tintColor"), 1, glm::value_ptr(color));
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
        }
    }

    if (drawWireframe)
    {
        glDisable(GL_BLEND);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(2.0f);

        for (int i = 0; i < static_cast<int>(colliders.size()); ++i)
        {
            if (!colliders[i].visible)
            {
                continue;
            }

            const glm::mat4 model = ComposeColliderMatrix(colliders[i]);
            glm::vec4 color = ResolveColor(colliders, collisionSystem, i, selectedIndex, cameraPosition, cameraRadius);
            color.a = 0.98f;

            glUniformMatrix4fv(shader->GetUniformLocation("model"), 1, GL_FALSE, glm::value_ptr(model));
            glUniform4fv(shader->GetUniformLocation("tintColor"), 1, glm::value_ptr(color));
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
        }

        if (previewCollider)
        {
            const glm::mat4 model = ComposeColliderMatrix(*previewCollider);
            const glm::vec4 color(1.0f, 0.86f, 0.22f, 0.98f);
            glUniformMatrix4fv(shader->GetUniformLocation("model"), 1, GL_FALSE, glm::value_ptr(model));
            glUniform4fv(shader->GetUniformLocation("tintColor"), 1, glm::value_ptr(color));
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
        }

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    if (drawThroughWalls)
    {
        glEnable(GL_DEPTH_TEST);
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

void ColliderRenderer::Shutdown()
{
    if (shader)
    {
        shader->Delete();
        delete shader;
        shader = nullptr;
    }

    if (EBO != 0)
    {
        glDeleteBuffers(1, &EBO);
        EBO = 0;
    }

    if (VBO != 0)
    {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }

    if (VAO != 0)
    {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
}

void ColliderRenderer::EnsureGeometry()
{
    if (VAO != 0)
    {
        return;
    }

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kCubeVertices), kCubeVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kCubeIndices), kCubeIndices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void*>(0));

    glBindVertexArray(0);
}
