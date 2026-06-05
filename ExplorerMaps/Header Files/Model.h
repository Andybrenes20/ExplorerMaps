#ifndef MODEL_CLASS_H
#define MODEL_CLASS_H

#include<cfloat>
#include<unordered_map>
#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<json/json.h>
#include<vector>
#include"Mesh.h"

using json = nlohmann::json;

struct aiNode;
struct aiScene;
struct aiMesh;

class Model
{
public:
	// Loads in a model from a file and stores tha information in 'data', 'JSON', and 'file'
	Model(const char* file);

	void Draw(
		Shader& shader,
		Camera& camera,
		const glm::mat4& worldTransform = glm::mat4(1.0f),
		bool cameraInsideStructure = false);
	glm::vec3 GetCenter() const;
	glm::vec3 GetBoundsMin() const;
	glm::vec3 GetBoundsMax() const;
	float GetRadius() const;
	glm::vec3 ResolveCollision(const glm::vec3& position, const glm::mat4& worldTransform, float radius) const;
	bool TrySnapToWalkableSurface(
		const glm::vec3& position,
		const glm::mat4& worldTransform,
		float probeRadius,
		float eyeHeight,
		float maxStepUp,
		float maxDropDown,
		float maxSlopeDegrees,
		glm::vec3& snappedPosition) const;
	glm::vec3 ResolveStructureCollision(
		const glm::vec3& targetPosition,
		const glm::vec3& previousPosition,
		const glm::mat4& worldTransform,
		float radius,
		float eyeHeight) const;
	bool IsInsideStructureVolume(
		const glm::vec3& position,
		const glm::mat4& worldTransform,
		float radius,
		float eyeHeight) const;

	struct TextureCacheInfo
	{
		enum class Kind
		{
			Solid = 0,
			File = 1,
			EmbeddedCompressed = 2,
			EmbeddedRgba = 3
		};

		Kind kind = Kind::Solid;
		std::string type = "diffuse";
		std::string path;
		glm::vec3 solidColor = glm::vec3(1.0f);
		int width = 1;
		int height = 1;
		std::vector<unsigned char> bytes;
	};

private:
	struct AssimpMeshBatch
	{
		std::vector<Vertex> vertices;
		std::vector<GLuint> indices;
		std::vector<Texture> textures;
		std::vector<TextureCacheInfo> textureInfos;
		glm::vec3 boundsMin = glm::vec3(FLT_MAX);
		glm::vec3 boundsMax = glm::vec3(-FLT_MAX);
	};
	struct CollisionMesh
	{
		std::vector<glm::vec3> vertices;
		std::vector<GLuint> indices;
		glm::vec3 boundsMin = glm::vec3(FLT_MAX);
		glm::vec3 boundsMax = glm::vec3(-FLT_MAX);
	};

	// Variables for easy access
	const char* file;
	std::vector<unsigned char> data;
	json JSON;
	glm::vec3 boundsMin = glm::vec3(FLT_MAX);
	glm::vec3 boundsMax = glm::vec3(-FLT_MAX);
	bool hasBounds = false;
	bool lightweightVehicleAsset = false;

	// All the meshes and transformations
	std::vector<Mesh> meshes;
	std::vector<glm::vec3> translationsMeshes;
	std::vector<glm::quat> rotationsMeshes;
	std::vector<glm::vec3> scalesMeshes;
	std::vector<glm::mat4> matricesMeshes;
	std::vector<glm::vec3> meshBoundsCenters;
	std::vector<float> meshBoundsRadii;
	std::vector<CollisionMesh> collisionMeshes;
	mutable int lastWalkableCollisionMeshIndex = -1;
	mutable int lastStructureCollisionMeshIndex = -1;
	std::vector<AssimpMeshBatch> assimpBatches;
	std::unordered_map<unsigned int, std::size_t> assimpBatchLookup;

	// Prevents textures from being loaded twice
	std::vector<std::string> loadedTexName;
	std::vector<Texture> loadedTex;
	std::vector<TextureCacheInfo> loadedTexInfo;

	// Loads a single mesh by its index
	void loadMesh(unsigned int indMesh);
	void loadAssimpModel(const char* path);
	void loadAssimpCollision(const char* path);
	bool tryLoadAssimpCache(const char* path);
	void saveAssimpCache(const char* path) const;
	void processAssimpCollisionScene(const aiScene* scene);
	void processAssimpNode(aiNode* node, const aiScene* scene, const glm::mat4& parentTransform, const std::string& fileDirectory);
	void processAssimpMesh(aiMesh* mesh, const aiScene* scene, const glm::mat4& transform, const std::string& fileDirectory);
	void finalizeAssimpBatches();
	void updateBounds(const glm::vec3& worldPosition);

	// Traverses a node recursively, so it essentially traverses all connected nodes
	void traverseNode(unsigned int nextNode, glm::mat4 matrix = glm::mat4(1.0f));

	// Gets the binary data from a file
	std::vector<unsigned char> getData();
	// Interprets the binary data into floats, indices, and textures
	std::vector<float> getFloats(json accessor);
	std::vector<GLuint> getIndices(json accessor);
	std::vector<Texture> getTextures();

	// Assembles all the floats into vertices
	std::vector<Vertex> assembleVertices
	(
		std::vector<glm::vec3> positions,
		std::vector<glm::vec3> normals,
		std::vector<glm::vec2> texUVs
	);

	// Helps with the assembly from above by grouping floats
	std::vector<glm::vec2> groupFloatsVec2(std::vector<float> floatVec);
	std::vector<glm::vec3> groupFloatsVec3(std::vector<float> floatVec);
	std::vector<glm::vec4> groupFloatsVec4(std::vector<float> floatVec);
};
#endif
