#include "SkateRampModel.h"

#include "PhysicsWorld.h"
#include "Shader.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <GL/glew.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/gtc/matrix_transform.hpp>

namespace {
    glm::vec3 ToVec3(const aiVector3D& value) {
        return glm::vec3(value.x, value.y, value.z);
    }
}

bool SkateRampModel::Load(const std::string& path) {
    Shutdown();

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        path,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_JoinIdenticalVertices);

    if (!scene || !scene->mRootNode || scene->mNumMeshes == 0) {
        return false;
    }

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    glm::vec3 boundsMin(std::numeric_limits<float>::max());
    glm::vec3 boundsMax(-std::numeric_limits<float>::max());

    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        aiMesh* mesh = scene->mMeshes[meshIndex];
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            const glm::vec3 position = ToVec3(mesh->mVertices[i]);
            boundsMin = glm::min(boundsMin, position);
            boundsMax = glm::max(boundsMax, position);
        }
    }

    const glm::vec3 center((boundsMin.x + boundsMax.x) * 0.5f, boundsMin.y, (boundsMin.z + boundsMax.z) * 0.5f);
    collisionVertices.clear();
    collisionIndices.clear();

    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        aiMesh* mesh = scene->mMeshes[meshIndex];
        const unsigned int vertexOffset = static_cast<unsigned int>(vertices.size());
        const unsigned int collisionVertexOffset = static_cast<unsigned int>(collisionVertices.size());
        const std::string meshName = mesh->mName.C_Str();
        const bool addRampCollision = meshName.find("Metal_Pipes") == std::string::npos;

        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            Vertex vertex{};
            vertex.position = ToVec3(mesh->mVertices[i]) - center;
            vertex.uv = mesh->HasTextureCoords(0)
                ? glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y)
                : glm::vec2(0.0f);
            vertex.normal = mesh->HasNormals()
                ? glm::normalize(ToVec3(mesh->mNormals[i]))
                : glm::vec3(0.0f, 1.0f, 0.0f);
            vertices.push_back(vertex);

            if (addRampCollision) {
                collisionVertices.push_back(vertex.position);
            }
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            const aiFace& face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                indices.push_back(vertexOffset + face.mIndices[j]);
                if (addRampCollision) {
                    collisionIndices.push_back(collisionVertexOffset + face.mIndices[j]);
                }
            }
        }
    }

    if (vertices.empty() || indices.empty()) {
        return false;
    }

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
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

    indexCount = static_cast<unsigned int>(indices.size());
    return true;
}

void SkateRampModel::Shutdown() {
    if (VAO != 0) glDeleteVertexArrays(1, &VAO);
    if (VBO != 0) glDeleteBuffers(1, &VBO);
    if (EBO != 0) glDeleteBuffers(1, &EBO);
    if (whiteTexture != 0) glDeleteTextures(1, &whiteTexture);
    VAO = VBO = EBO = whiteTexture = 0;
    indexCount = 0;
    collisionVertices.clear();
    collisionIndices.clear();
}

void SkateRampModel::Draw(Shader& shader, const glm::vec3& position, float yawDegrees, float scale, const glm::vec3& tint) const {
    if (VAO == 0 || indexCount == 0) {
        return;
    }

    const glm::mat4 model =
        glm::translate(glm::mat4(1.0f), position) *
        glm::rotate(glm::mat4(1.0f), glm::radians(yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(scale));

    shader.setMat4("model", model);
    shader.setVec3("objectTint", tint);
    shader.setFloat("objectAlpha", 1.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, whiteTexture);
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void SkateRampModel::AddCollisionTo(PhysicsWorld& physics, const glm::vec3& position, float yawDegrees, float scale) const {
    if (collisionVertices.empty() || collisionIndices.empty()) {
        return;
    }

    const glm::mat4 transform =
        glm::translate(glm::mat4(1.0f), position) *
        glm::rotate(glm::mat4(1.0f), glm::radians(yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(scale));

    std::vector<glm::vec3> transformedVertices;
    transformedVertices.reserve(collisionVertices.size());
    for (const glm::vec3& vertex : collisionVertices) {
        transformedVertices.push_back(glm::vec3(transform * glm::vec4(vertex, 1.0f)));
    }

    physics.AddCollisionTriangles(transformedVertices, collisionIndices);
}
