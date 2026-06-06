#include "PhysicsWorld.h"
#include "Optimization.h"
#include <iostream>
#include <GL/glew.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

extern float currentLoadingProgress;

bool PhysicsWorld::LoadCollisionData(const std::string& path) {
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
    lastRaycastMeshIndex = -1;

    unsigned int totalMeshes = scene->mNumMeshes;
    ProcessNode(scene->mRootNode, scene);

    currentLoadingProgress = 1.0f;
    return true; // Terminó la CPU, pero aún falta subir a la GPU
}

void PhysicsWorld::ProcessNode(aiNode* node, const aiScene* scene) {
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        ProcessMesh(mesh, scene);
    }
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        ProcessNode(node->mChildren[i], scene);
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

void PhysicsWorld::ProcessMesh(aiMesh* mesh, const aiScene* scene) {
    unsigned int vertexOffset = static_cast<unsigned int>(cityCollider.vertices.size());
    CollisionSubMesh collisionSubMesh;
    collisionSubMesh.indexStart = cityCollider.indices.size();

    // 1. Física
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        const glm::vec3 position(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        cityCollider.vertices.push_back(position);
        if (i == 0) {
            collisionSubMesh.boundsMin = position;
            collisionSubMesh.boundsMax = position;
        }
        else {
            collisionSubMesh.boundsMin = glm::min(collisionSubMesh.boundsMin, position);
            collisionSubMesh.boundsMax = glm::max(collisionSubMesh.boundsMax, position);
        }
    }
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            cityCollider.indices.push_back(face.mIndices[j] + vertexOffset);
        }
    }

    // 2. Extracción a Memoria Temporal (RAM)
    collisionSubMesh.indexCount = cityCollider.indices.size() - collisionSubMesh.indexStart;
    if (collisionSubMesh.indexCount > 0) {
        collisionSubMeshes.push_back(collisionSubMesh);
    }

    RenderMesh currentVisualMesh;

    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        currentVisualMesh.tempVertices.push_back(glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z));

        if (mesh->mTextureCoords[0]) {
            currentVisualMesh.tempUVs.push_back(glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y));
        }
        else {
            currentVisualMesh.tempUVs.push_back(glm::vec2(0.0f, 0.0f));
        }

        if (mesh->HasNormals()) {
            currentVisualMesh.tempNormals.push_back(glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z));
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
    currentLoadingProgress = static_cast<float>(visualMeshes.size()) / static_cast<float>(scene->mNumMeshes);
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

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            stbi_image_free(tex.pixels); // Liberar RAM
            tex.pixels = nullptr;
        }
    }

    // 2. Subir Geometría
    for (auto& mesh : visualMeshes) {
        // Enlazar ID de textura generado
        if (mesh.pendingTextureIndex != -1) {
            mesh.textureID = pendingTextures[mesh.pendingTextureIndex].gpuID;
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
