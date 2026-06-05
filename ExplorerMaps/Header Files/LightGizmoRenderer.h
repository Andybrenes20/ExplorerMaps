#pragma once

#include <memory>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "EditorTypes.h"
#include "shaderClass.h"

struct EditorViewportRenderRequest;

class LightGizmoRenderer
{
public:
    void Init();
    void Shutdown();
    void Render(const std::vector<Light>& lights, const EditorViewportRenderRequest& request) const;

private:
    void DrawPrimitive(GLuint vao, GLsizei vertexCount, const glm::mat4& modelMatrix, const glm::vec4& color) const;
    void CreateGeometry();
    void DestroyGeometry();

    std::unique_ptr<Shader> lineShader;
    GLuint cubeLinesVao = 0;
    GLuint cubeLinesVbo = 0;
    GLuint sphereLinesVao = 0;
    GLuint sphereLinesVbo = 0;
    GLuint iconLinesVao = 0;
    GLuint iconLinesVbo = 0;
    GLsizei cubeLineVertexCount = 0;
    GLsizei sphereLineVertexCount = 0;
    GLsizei iconLineVertexCount = 0;
};
