#ifndef SKYBOX_CLASS_H
#define SKYBOX_CLASS_H

#include <string>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Camara.h"
#include "shaderClass.h"

struct SkyCloudSettings
{
	float coverage = 0.62f;
	float speed = 1.0f;
	float crispiness = 1.0f;
	float curliness = 0.75f;
	float density = 0.95f;
};

class Skybox
{
public:
	Skybox(
		const std::vector<std::string>& dayFacePaths,
		const std::vector<std::string>& nightFacePaths,
		const char* vertexShaderPath,
		const char* fragmentShaderPath
	);
	~Skybox();

	void Draw(const Camera& camera, float FOVdeg, float nearPlane, float farPlane, float time, float sunHeight, const glm::vec3& sunDirection);
	void SetBlendFactor(float factor);
	void SetCloudSettings(const SkyCloudSettings& settings);
	static std::vector<unsigned char> BuildProceduralFace(int width, int height, int faceIndex, bool nightTheme);

private:
	GLuint VAO = 0;
	GLuint VBO = 0;
	GLuint dayCubemapTexture = 0;
	GLuint nightCubemapTexture = 0;
	float blendFactor = 0.0f;
	SkyCloudSettings cloudSettings;
	Shader shader;

	void setupCube();
	static glm::vec3 DirectionForFace(int faceIndex, float u, float v);
};

#endif
