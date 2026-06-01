#ifndef VOLUMETRIC_CLOUD_RENDERER_H
#define VOLUMETRIC_CLOUD_RENDERER_H

#include <array>
#include <string>
#include <unordered_map>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Camara.h"
#include "Skybox.h"
#include "shaderClass.h"

class VolumetricCloudRenderer
{
public:
	VolumetricCloudRenderer(int screenWidth, int screenHeight);
	~VolumetricCloudRenderer();

	bool IsReady() const;
	const std::string& GetStatusMessage() const;
	void Shutdown();

	void BindSceneFramebuffer(const glm::vec3& clearColor);
	void BindSkyFramebuffer(const glm::vec3& clearColor);
	void RenderTerrainSky(
		const glm::mat4& view,
		const glm::mat4& projection,
		const glm::vec3& lightDirection,
		const glm::vec3& skyColorTop,
		const glm::vec3& skyColorBottom);
	void RenderClouds(
		const Camera& camera,
		const glm::mat4& view,
		const glm::mat4& projection,
		const SkyCloudSettings& settings,
		const glm::vec3& lightDirection,
		const glm::vec3& lightColor,
		const glm::vec3& skyColorTop,
		const glm::vec3& skyColorBottom,
		float time);
	void CompositeToScreen(const SkyCloudSettings& settings, bool wireframe);

private:
	struct Framebuffer
	{
		GLuint fbo = 0;
		GLuint color = 0;
		GLuint depth = 0;
		int width = 0;
		int height = 0;
	};

	class ComputeProgram
	{
	public:
		ComputeProgram() = default;
		explicit ComputeProgram(const char* computeShaderPath);
		~ComputeProgram();

		bool Load(const char* computeShaderPath);
		bool IsValid() const;
		void Use() const;
		GLint GetUniformLocation(const char* uniformName);
		GLuint GetId() const;
		void Reset();

	private:
		GLuint id = 0;
		std::unordered_map<std::string, GLint> uniformLocationCache;
	};

	enum CloudTextureIndex
	{
		CloudColor = 0,
		CloudBloom = 1,
		CloudAlpha = 2,
		CloudDistance = 3,
		CloudTextureCount = 4
	};

	bool LoadComputeFunctions();
	bool CreateFramebuffers();
	bool CreateCloudTextures();
	bool CreateNoiseTextures();
	bool CreateFullscreenQuad();
	void GenerateModelTextures();
	void GenerateWeatherMap(const glm::vec3& seed, float perlinFrequency);
	void ReleaseFramebuffer(Framebuffer& framebuffer);
	void BindTexture2D(GLuint texture, GLuint unit) const;
	void BindTexture3D(GLuint texture, GLuint unit) const;
	void DrawFullscreenQuad() const;
	GLuint GetCloudOutputTexture(const SkyCloudSettings& settings) const;

	int screenWidth = 0;
	int screenHeight = 0;
	bool ready = false;
	std::string statusMessage;

	Framebuffer sceneFramebuffer;
	Framebuffer skyFramebuffer;
	Framebuffer cloudPostFramebuffer;
	std::array<GLuint, CloudTextureCount> cloudTextures{};
	GLuint perlinWorleyTexture = 0;
	GLuint worleyTexture = 0;
	GLuint weatherTexture = 0;
	GLuint quadVAO = 0;
	GLuint quadVBO = 0;
	glm::vec3 weatherSeed = glm::vec3(0.0f);
	float weatherFrequency = -1.0f;

	ComputeProgram perlinWorleyProgram;
	ComputeProgram worleyProgram;
	ComputeProgram weatherProgram;
	ComputeProgram volumetricProgram;
	Shader cloudPostShader;
	Shader compositeShader;
	Shader terrainSkyShader;
};

#endif
