#include "LightGizmoRenderer.h"

#include <array>
#include <vector>

#include "Editor.h"

namespace
{
    std::vector<float> BuildCubeLines()
    {
        const std::array<glm::vec3, 8> corners =
        {
            glm::vec3(-1.0f, -1.0f, -1.0f),
            glm::vec3( 1.0f, -1.0f, -1.0f),
            glm::vec3( 1.0f,  1.0f, -1.0f),
            glm::vec3(-1.0f,  1.0f, -1.0f),
            glm::vec3(-1.0f, -1.0f,  1.0f),
            glm::vec3( 1.0f, -1.0f,  1.0f),
            glm::vec3( 1.0f,  1.0f,  1.0f),
            glm::vec3(-1.0f,  1.0f,  1.0f)
        };

        const int edges[][2] =
        {
            { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
            { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
            { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
        };

        std::vector<float> vertices;
        for (const auto& edge : edges)
        {
            for (const int index : edge)
            {
                vertices.push_back(corners[index].x);
                vertices.push_back(corners[index].y);
                vertices.push_back(corners[index].z);
            }
        }

        return vertices;
    }

    std::vector<float> BuildSphereLines(const int segments)
    {
        const float tau = 6.28318530718f;
        std::vector<float> vertices;
        for (int ring = 0; ring < 3; ++ring)
        {
            for (int i = 0; i < segments; ++i)
            {
                const float angleA = tau * static_cast<float>(i) / static_cast<float>(segments);
                const float angleB = tau * static_cast<float>(i + 1) / static_cast<float>(segments);

                glm::vec3 pointA;
                glm::vec3 pointB;
                if (ring == 0)
                {
                    pointA = glm::vec3(std::cos(angleA), std::sin(angleA), 0.0f);
                    pointB = glm::vec3(std::cos(angleB), std::sin(angleB), 0.0f);
                }
                else if (ring == 1)
                {
                    pointA = glm::vec3(std::cos(angleA), 0.0f, std::sin(angleA));
                    pointB = glm::vec3(std::cos(angleB), 0.0f, std::sin(angleB));
                }
                else
                {
                    pointA = glm::vec3(0.0f, std::cos(angleA), std::sin(angleA));
                    pointB = glm::vec3(0.0f, std::cos(angleB), std::sin(angleB));
                }

                vertices.insert(vertices.end(), { pointA.x, pointA.y, pointA.z, pointB.x, pointB.y, pointB.z });
            }
        }

        return vertices;
    }

    std::vector<float> BuildLightIconLines()
    {
        return
        {
             0.0f,  1.4f, 0.0f,   0.0f, -1.4f, 0.0f,
            -1.4f,  0.0f, 0.0f,   1.4f,  0.0f, 0.0f,
             0.0f,  0.0f,-1.4f,   0.0f,  0.0f, 1.4f,
            -0.8f,  0.8f, 0.0f,   0.8f, -0.8f, 0.0f,
            -0.8f, -0.8f, 0.0f,   0.8f,  0.8f, 0.0f
        };
    }

    void UploadLineGeometry(GLuint& vao, GLuint& vbo, GLsizei& vertexCount, const std::vector<float>& vertices)
    {
        vertexCount = static_cast<GLsizei>(vertices.size() / 3);
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
    }
}

void LightGizmoRenderer::Init()
{
    lineShader = std::make_unique<Shader>("Shaders/editor_line.vert", "Shaders/editor_line.frag");
    CreateGeometry();
}

void LightGizmoRenderer::Shutdown()
{
    DestroyGeometry();
    lineShader.reset();
}

void LightGizmoRenderer::Render(const std::vector<Light>& lights, const EditorViewportRenderRequest& request) const
{
    if (!lineShader)
    {
        return;
    }

    lineShader->Activate();
    glUniformMatrix4fv(lineShader->GetUniformLocation("view"), 1, GL_FALSE, &request.view[0][0]);
    glUniformMatrix4fv(lineShader->GetUniformLocation("projection"), 1, GL_FALSE, &request.projection[0][0]);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (const Light& light : lights)
    {
        const glm::vec4 baseColor = light.selected
            ? glm::vec4(1.0f, 0.92f, 0.35f, 1.0f)
            : glm::vec4(light.color, 0.95f);
        const glm::vec4 volumeColor = glm::vec4(baseColor.r, baseColor.g, baseColor.b, light.selected ? 0.95f : 0.55f);
        const glm::vec4 helperColor = glm::vec4(baseColor.r, baseColor.g, baseColor.b, 1.0f);

        if (light.visualType == LightVisualType::Cube)
        {
            DrawPrimitive(
                cubeLinesVao,
                cubeLineVertexCount,
                ComposeTransformMatrix(light.position, glm::vec3(0.0f), glm::max(light.boxSize, glm::vec3(1.0f))),
                volumeColor);
        }
        else
        {
            DrawPrimitive(
                sphereLinesVao,
                sphereLineVertexCount,
                ComposeTransformMatrix(light.position, glm::vec3(0.0f), glm::vec3(std::max(light.radius, 1.0f))),
                volumeColor);
        }
        DrawPrimitive(
            cubeLinesVao,
            cubeLineVertexCount,
            ComposeTransformMatrix(light.position, glm::vec3(0.0f), glm::vec3(std::max(light.helperSize, 1.0f))),
            helperColor);
        DrawPrimitive(
            iconLinesVao,
            iconLineVertexCount,
            ComposeTransformMatrix(light.position, glm::vec3(0.0f), glm::vec3(std::max(light.helperSize * 0.35f, 1.5f))),
            helperColor);
    }

    glDisable(GL_BLEND);
}

void LightGizmoRenderer::DrawPrimitive(GLuint vao, GLsizei vertexCount, const glm::mat4& modelMatrix, const glm::vec4& color) const
{
    glUniformMatrix4fv(lineShader->GetUniformLocation("model"), 1, GL_FALSE, &modelMatrix[0][0]);
    glUniform4f(lineShader->GetUniformLocation("color"), color.r, color.g, color.b, color.a);
    glBindVertexArray(vao);
    glDrawArrays(GL_LINES, 0, vertexCount);
    glBindVertexArray(0);
}

void LightGizmoRenderer::CreateGeometry()
{
    UploadLineGeometry(cubeLinesVao, cubeLinesVbo, cubeLineVertexCount, BuildCubeLines());
    UploadLineGeometry(sphereLinesVao, sphereLinesVbo, sphereLineVertexCount, BuildSphereLines(48));
    UploadLineGeometry(iconLinesVao, iconLinesVbo, iconLineVertexCount, BuildLightIconLines());
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void LightGizmoRenderer::DestroyGeometry()
{
    if (cubeLinesVbo != 0)
    {
        glDeleteBuffers(1, &cubeLinesVbo);
        cubeLinesVbo = 0;
    }
    if (cubeLinesVao != 0)
    {
        glDeleteVertexArrays(1, &cubeLinesVao);
        cubeLinesVao = 0;
    }
    if (sphereLinesVbo != 0)
    {
        glDeleteBuffers(1, &sphereLinesVbo);
        sphereLinesVbo = 0;
    }
    if (sphereLinesVao != 0)
    {
        glDeleteVertexArrays(1, &sphereLinesVao);
        sphereLinesVao = 0;
    }
    if (iconLinesVbo != 0)
    {
        glDeleteBuffers(1, &iconLinesVbo);
        iconLinesVbo = 0;
    }
    if (iconLinesVao != 0)
    {
        glDeleteVertexArrays(1, &iconLinesVao);
        iconLinesVao = 0;
    }
}
