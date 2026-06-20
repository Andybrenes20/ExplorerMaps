#pragma once
#include <vector>
#include <string>
#include <map>
#include <cstddef>
#include <glm/glm.hpp>
#include <assimp/Importer.hpp>
#include <assimp/matrix4x4.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

struct CollisionMesh {
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> uvs;
    std::vector<unsigned int> indices;
};

struct CollisionSubMesh {
    std::size_t indexStart = 0;
    std::size_t indexCount = 0;
    glm::vec3 boundsMin = glm::vec3(0.0f);
    glm::vec3 boundsMax = glm::vec3(0.0f);
    struct BvhNode {
        glm::vec3 boundsMin = glm::vec3(0.0f);
        glm::vec3 boundsMax = glm::vec3(0.0f);
        int left = -1;
        int right = -1;
        std::size_t firstTriangle = 0;
        std::size_t triangleCount = 0;
    };
    std::vector<BvhNode> bvhNodes;
    std::vector<std::size_t> bvhTriangleOffsets;
};

// Estructura para almacenar los píxeles crudos en RAM temporalmente
struct RawTextureData {
    int width = 0, height = 0, nrChannels = 0;
    unsigned char* pixels = nullptr;
    unsigned int gpuID = 0;
};

struct RenderMesh {
    unsigned int VAO = 0, VBO_Pos = 0, VBO_UV = 0, VBO_Normal = 0, EBO = 0;
    unsigned int numIndices = 0;
    unsigned int textureID = 0;
    int pendingTextureIndex = -1; // Para vincular la textura más tarde

    // Datos temporales en la RAM de la CPU
    std::vector<glm::vec3> tempVertices;
    std::vector<glm::vec2> tempUVs;
    std::vector<glm::vec3> tempNormals;
    std::vector<unsigned int> tempIndices;

    glm::vec3 diffuseColor;
    glm::vec3 specularColor;
    float shineness;
    std::string nodeName;
    std::string materialName;
    glm::vec3 pivot = glm::vec3(0.0f);
    bool addedContent = false;
};

struct DynamicCollisionBox {
    glm::vec3 center = glm::vec3(0.0f);
    glm::vec3 size = glm::vec3(1.0f);
    float yaw = 0.0f;
};

class PhysicsWorld {
public:
    CollisionMesh cityCollider;
    std::vector<CollisionSubMesh> collisionSubMeshes;
    std::vector<RenderMesh> visualMeshes;

    bool LoadCollisionData(const std::string& path);
    bool LoadVisualData(const std::string& path);
    bool Raycast(glm::vec3 origin, glm::vec3 direction, float& outDistance);
    bool RaycastStatic(glm::vec3 origin, glm::vec3 direction, float& outDistance);
    void AddCollisionBox(const glm::vec3& center, const glm::vec3& size);
    void AddCollisionTriangles(const std::vector<glm::vec3>& vertices, const std::vector<unsigned int>& indices);
    void SetDynamicCollisionBoxes(const std::vector<DynamicCollisionBox>& boxes);

    // --- NUEVA FUNCIÓN: Se ejecuta en el Hilo Principal ---
    void UploadToGPU();

private:
    std::vector<DynamicCollisionBox> dynamicCollisionBoxes;
    std::map<int, RawTextureData> pendingTextures;
    int lastRaycastMeshIndex = -1;
    bool visualOnly = false;
    bool forceNodeTransforms = false;

    bool LoadData(const std::string& path);
    void ProcessNode(aiNode* node, const aiScene* scene, const aiMatrix4x4& parentTransform);
    void ProcessMesh(aiMesh* mesh, const aiScene* scene, const aiMatrix4x4& transform, float scale, const std::string& nodeName, bool addedContent);
    void LoadSingleEmbeddedTexture(const aiTexture* texture, int textureIndex);
    bool IntersectRayTriangle(glm::vec3 orig, glm::vec3 dir, glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, float& t);
};
