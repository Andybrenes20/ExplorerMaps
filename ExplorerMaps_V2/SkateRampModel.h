#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

class Shader;
class PhysicsWorld;

class SkateRampModel {
public:
    bool Load(const std::string& path);
    void Shutdown();
    void Draw(Shader& shader, const glm::vec3& position, float yawDegrees, float scale, const glm::vec3& tint) const;
    void AddCollisionTo(PhysicsWorld& physics, const glm::vec3& position, float yawDegrees, float scale) const;

private:
    struct Vertex {
        glm::vec3 position;
        glm::vec2 uv;
        glm::vec3 normal;
    };

    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int EBO = 0;
    unsigned int whiteTexture = 0;
    unsigned int indexCount = 0;
    std::vector<glm::vec3> collisionVertices;
    std::vector<unsigned int> collisionIndices;
};
