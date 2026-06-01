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
	float coverage = 0.45f;
	float speed = 1.0f;
	float crispiness = 1.0f;
	float curliness = 0.10f;
	float density = 1.0f;
	float lightAbsorption = 0.35f;
	float skyDomeRadius = 600000.0f;
	float cloudBottom = 5000.0f;
	float cloudTop = 17000.0f;
	float cloudFrequency = 0.80f;
	glm::vec3 cloudColor = glm::vec3(98.0f, 105.0f, 120.0f) / 255.0f;
	float rainIntensity = 0.0f;
	glm::vec3 cameraPosition = glm::vec3(0.0f);
	bool postProcessing = true;
	bool godRays = false;
	bool sugarPowder = false;
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

	void Draw(const Camera& camera, float FOVdeg, float nearPlane, float farPlane, float time, float sunHeight, const glm::vec3& sunDirection, const glm::vec3& moonPosition);
	void SetBlendFactor(float factor);
	void SetCloudSettings(const SkyCloudSettings& settings);
	void SetProceduralCloudsEnabled(bool enabled);
	static std::vector<unsigned char> BuildProceduralFace(int width, int height, int faceIndex, bool nightTheme);

private:
	GLuint VAO = 0;
	GLuint VBO = 0;
	GLuint dayCubemapTexture = 0;
	GLuint nightCubemapTexture = 0;
	float blendFactor = 0.0f;
	SkyCloudSettings cloudSettings;
	bool proceduralCloudsEnabled = true;
	Shader shader;

	void setupCube();
	static glm::vec3 DirectionForFace(int faceIndex, float u, float v);
};

#endif
