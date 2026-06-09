#include "PhysicsWorld.h"
#include "Optimization.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <GL/glew.h>
#include <assimp/matrix3x3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

extern float currentLoadingProgress;

namespace {
constexpr unsigned int COLLISION_FACES_PER_CHUNK = 1024;
constexpr unsigned int LEGACY_CITY_MESH_COUNT = 2780;
constexpr float ADDED_CONTENT_SCALE = 100.0f;

glm::vec3 TransformPosition(const aiMatrix4x4& transform, const aiVector3D& position, float scale) {
    return scale * glm::vec3(
        transform.a1 * position.x + transform.a2 * position.y + transform.a3 * position.z + transform.a4,
        transform.b1 * position.x + transform.b2 * position.y + transform.b3 * position.z + transform.b4,
        transform.c1 * position.x + transform.c2 * position.y + transform.c3 * position.z + transform.c4);
}

glm::vec3 TransformNormal(const aiMatrix3x3& transform, const aiVector3D& normal) {
    const glm::vec3 transformed(
        transform.a1 * normal.x + transform.a2 * normal.y + transform.a3 * normal.z,
        transform.b1 * normal.x + transform.b2 * normal.y + transform.b3 * normal.z,
        transform.c1 * normal.x + transform.c2 * normal.y + transform.c3 * normal.z);

    return glm::length(transformed) > 0.0001f
        ? glm::normalize(transformed)
        : glm::vec3(0.0f, 1.0f, 0.0f);
}
}

bool PhysicsWorld::LoadCollisionData(const std::string& path) {
    visualOnly = false;
    forceNodeTransforms = false;
    return LoadData(path);
}

bool PhysicsWorld::LoadVisualData(const std::string& path) {
    visualOnly = true;
    forceNodeTransforms = true;
    return LoadData(path);
}

bool PhysicsWorld::LoadData(const std::string& path) {
    currentLoadingProgress = 0.01f;

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        path,
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenSmoothNormals);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "ERROR::ASSIMP:: " << importer.GetErrorString() << std::endl;
        return false;
    }

    visualMeshes.clear();
    collisionSubMeshes.clear();
    cityCollider.vertices.clear();
    cityCollider.indices.clear();
    pendingTextures.clear();
    lastRaycastMeshIndex = -1;

    ProcessNode(scene->mRootNode, scene, aiMatrix4x4());
    if (!visualOnly) {
        const std::size_t acceleratedMeshes = Optimization::BuildCollisionAcceleration(cityCollider, collisionSubMeshes);
        std::cout << "Collision BVH: " << acceleratedMeshes << " mallas pesadas aceleradas." << std::endl;
    }

    currentLoadingProgress = 1.0f;
    return true; // Terminó la CPU, pero aún falta subir a la GPU
}

void PhysicsWorld::ProcessNode(aiNode* node, const aiScene* scene, const aiMatrix4x4& parentTransform) {
    const aiMatrix4x4 transform = parentTransform * node->mTransformation;

    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        const unsigned int meshIndex = node->mMeshes[i];
        aiMesh* mesh = scene->mMeshes[meshIndex];

        // La ciudad original ya tiene sus vertices en unidades caminables.
        // El contenido agregado conserva transformaciones y unidades de Blender.
        const bool isAddedContent = forceNodeTransforms || meshIndex >= LEGACY_CITY_MESH_COUNT;
        ProcessMesh(
            mesh,
            scene,
            isAddedContent ? transform : aiMatrix4x4(),
            forceNodeTransforms ? 1.0f : (isAddedContent ? ADDED_CONTENT_SCALE : 1.0f),
            node->mName.C_Str());
    }
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        ProcessNode(node->mChildren[i], scene, transform);
    }
}

// NUEVO: Solo decodifica a RAM, NO toca OpenGL
void PhysicsWorld::LoadSingleEmbeddedTexture(const aiTexture* texture, int textureIndex) {
    RawTextureData texData;
    stbi_set_flip_vertically_on_load(false);

    if (texture->mHeight == 0) {
        texData.pixels = stbi_load_from_memory(reinterpret_cast<unsigned char*>(texture->pcData), texture->mWidth, &texData.width, &texData.height, &texData.nrChannels, 0);
    }
    else {
        texData.pixels = stbi_load_from_memory(reinterpret_cast<unsigned char*>(texture->pcData), texture->mWidth * texture->mHeight * 4, &texData.width, &texData.height, &texData.nrChannels, 0);
    }

    if (texData.pixels) {
        pendingTextures[textureIndex] = texData;
    }
    else {
        std::cout << "Error decodificando textura en RAM." << std::endl;
    }
}

void PhysicsWorld::ProcessMesh(aiMesh* mesh, const aiScene* scene, const aiMatrix4x4& transform, float scale, const std::string& nodeName) {
    const unsigned int vertexOffset = static_cast<unsigned int>(cityCollider.vertices.size());
    aiMatrix3x3 normalTransform(transform);
    normalTransform.Inverse().Transpose();

    // 1. Física
    for (unsigned int i = 0; !visualOnly && i < mesh->mNumVertices; i++) {
        const glm::vec3 position = TransformPosition(transform, mesh->mVertices[i], scale);
        cityCollider.vertices.push_back(position);
    }

    for (unsigned int chunkStart = 0; !visualOnly && chunkStart < mesh->mNumFaces; chunkStart += COLLISION_FACES_PER_CHUNK) {
        CollisionSubMesh collisionChunk;
        collisionChunk.indexStart = cityCollider.indices.size();
        bool hasBounds = false;
        const unsigned int chunkEnd = std::min(chunkStart + COLLISION_FACES_PER_CHUNK, mesh->mNumFaces);

        for (unsigned int i = chunkStart; i < chunkEnd; i++) {
            const aiFace& face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                const unsigned int index = face.mIndices[j] + vertexOffset;
                cityCollider.indices.push_back(index);
                const glm::vec3& position = cityCollider.vertices[index];
                if (!hasBounds) {
                    collisionChunk.boundsMin = position;
                    collisionChunk.boundsMax = position;
                    hasBounds = true;
                }
                else {
                    collisionChunk.boundsMin = glm::min(collisionChunk.boundsMin, position);
                    collisionChunk.boundsMax = glm::max(collisionChunk.boundsMax, position);
                }
            }
        }

        collisionChunk.indexCount = cityCollider.indices.size() - collisionChunk.indexStart;
        if (collisionChunk.indexCount > 0) {
            collisionSubMeshes.push_back(collisionChunk);
        }
    }

    // 2. Extracción a Memoria Temporal (RAM)
    RenderMesh currentVisualMesh;
    currentVisualMesh.nodeName = nodeName;
    currentVisualMesh.pivot = TransformPosition(transform, aiVector3D(0.0f), scale);

    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        currentVisualMesh.tempVertices.push_back(TransformPosition(transform, mesh->mVertices[i], scale));

        if (mesh->mTextureCoords[0]) {
            currentVisualMesh.tempUVs.push_back(glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y));
        }
        else {
            currentVisualMesh.tempUVs.push_back(glm::vec2(0.0f, 0.0f));
        }

        if (mesh->HasNormals()) {
            currentVisualMesh.tempNormals.push_back(TransformNormal(normalTransform, mesh->mNormals[i]));
        }
        else {
            currentVisualMesh.tempNormals.push_back(glm::vec3(0.0f, 1.0f, 0.0f));
        }
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            currentVisualMesh.tempIndices.push_back(face.mIndices[j]);
        }
    }
    currentVisualMesh.numIndices = static_cast<unsigned int>(currentVisualMesh.tempIndices.size());

    // 3. Extracción de Material
    currentVisualMesh.diffuseColor = glm::vec3(0.5f, 0.5f, 0.5f);
    currentVisualMesh.specularColor = glm::vec3(0.2f, 0.2f, 0.2f);
    currentVisualMesh.shineness = 32.0f;

    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        aiString texturePath;
        bool foundTexture = false;

        if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
            foundTexture = true;
        }
        else if (material->GetTexture(aiTextureType_EMISSIVE, 0, &texturePath) == AI_SUCCESS) {
            foundTexture = true;
        }

        if (foundTexture) {
            const char* pathStr = texturePath.C_Str();
            if (pathStr[0] == '*') {
                int textureIndex = atoi(&pathStr[1]);
                if (pendingTextures.find(textureIndex) == pendingTextures.end()) {
                    LoadSingleEmbeddedTexture(scene->mTextures[textureIndex], textureIndex);
                }
                currentVisualMesh.pendingTextureIndex = textureIndex; // Se enlazará en UploadToGPU
            }
        }

        aiColor4D color;
        if (aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &color) == AI_SUCCESS) {
            currentVisualMesh.diffuseColor = glm::vec3(color.r, color.g, color.b);
        }
        if (aiGetMaterialColor(material, AI_MATKEY_COLOR_SPECULAR, &color) == AI_SUCCESS) {
            currentVisualMesh.specularColor = glm::vec3(color.r, color.g, color.b);
        }
    }

    // NO HAY LLAMADAS A OPENGL AQUÍ
    visualMeshes.push_back(currentVisualMesh);
    currentLoadingProgress = std::clamp(
        static_cast<float>(visualMeshes.size()) / static_cast<float>(scene->mNumMeshes),
        0.0f,
        0.99f);
}

// --- NUEVA FUNCIÓN: Toma la RAM y la envía a la VRAM de forma segura ---
void PhysicsWorld::UploadToGPU() {
    // 1. Subir Texturas
    for (auto& pair : pendingTextures) {
        RawTextureData& tex = pair.second;
        if (tex.pixels) {
            glGenTextures(1, &tex.gpuID);
            glBindTexture(GL_TEXTURE_2D, tex.gpuID);

            GLenum format = (tex.nrChannels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_2D, 0, format, tex.width, tex.height, 0, format, GL_UNSIGNED_BYTE, tex.pixels);
            glGenerateMipmap(GL_TEXTURE_2D);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            stbi_image_free(tex.pixels); // Liberar RAM
            tex.pixels = nullptr;
        }
    }

    // 2. Subir Geometría
    std::map<unsigned int, unsigned int> fallbackTextures;
    for (auto& mesh : visualMeshes) {
        // Enlazar ID de textura generado
        if (mesh.pendingTextureIndex != -1) {
            mesh.textureID = pendingTextures[mesh.pendingTextureIndex].gpuID;
        }
        else {
            const auto channel = [](float value) {
                return static_cast<unsigned char>(std::clamp(std::lround(value * 255.0f), 0l, 255l));
            };
            const unsigned char color[] = {
                channel(mesh.diffuseColor.r),
                channel(mesh.diffuseColor.g),
                channel(mesh.diffuseColor.b),
                255
            };
            const unsigned int colorKey =
                (static_cast<unsigned int>(color[0]) << 16) |
                (static_cast<unsigned int>(color[1]) << 8) |
                static_cast<unsigned int>(color[2]);

            auto fallback = fallbackTextures.find(colorKey);
            if (fallback == fallbackTextures.end()) {
                glGenTextures(1, &mesh.textureID);
                glBindTexture(GL_TEXTURE_2D, mesh.textureID);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, color);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                fallbackTextures[colorKey] = mesh.textureID;
            }
            else {
                mesh.textureID = fallback->second;
            }
        }

        glGenVertexArrays(1, &mesh.VAO);
        glGenBuffers(1, &mesh.VBO_Pos);
        glGenBuffers(1, &mesh.VBO_UV);
        glGenBuffers(1, &mesh.VBO_Normal);
        glGenBuffers(1, &mesh.EBO);

        glBindVertexArray(mesh.VAO);

        glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO_Pos);
        glBufferData(GL_ARRAY_BUFFER, mesh.tempVertices.size() * sizeof(glm::vec3), mesh.tempVertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO_UV);
        glBufferData(GL_ARRAY_BUFFER, mesh.tempUVs.size() * sizeof(glm::vec2), mesh.tempUVs.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO_Normal);
        glBufferData(GL_ARRAY_BUFFER, mesh.tempNormals.size() * sizeof(glm::vec3), mesh.tempNormals.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(2);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.tempIndices.size() * sizeof(unsigned int), mesh.tempIndices.data(), GL_STATIC_DRAW);

        // Limpiar vectores temporales de la RAM
        mesh.tempVertices.clear();
        mesh.tempUVs.clear();
        mesh.tempNormals.clear();
        mesh.tempIndices.clear();
    }
}

// LÓGICA DE RAYCAST SE MANTIENE EXACTAMENTE IGUAL
bool PhysicsWorld::IntersectRayTriangle(glm::vec3 orig, glm::vec3 dir, glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, float& t) {
    glm::vec3 e1 = v1 - v0; glm::vec3 e2 = v2 - v0; glm::vec3 pvec = glm::cross(dir, e2); float det = glm::dot(e1, pvec);
    const float EPSILON = 0.0000001f; if (det > -EPSILON && det < EPSILON) return false; float invDet = 1.0f / det;
    glm::vec3 tvec = orig - v0; float u = glm::dot(tvec, pvec) * invDet; if (u < 0.0f || u > 1.0f) return false;
    glm::vec3 qvec = glm::cross(tvec, e1); float v = glm::dot(dir, qvec) * invDet; if (v < 0.0f || u + v > 1.0f) return false;
    t = glm::dot(e2, qvec) * invDet; return t > EPSILON;
}

bool PhysicsWorld::Raycast(glm::vec3 origin, glm::vec3 direction, float& outDistance) {
    if (glm::length(direction) <= 0.0001f) {
        return false;
    }

    return Optimization::RaycastCollisionMeshes(
        cityCollider,
        collisionSubMeshes,
        origin,
        glm::normalize(direction),
        outDistance,
        lastRaycastMeshIndex);
}
